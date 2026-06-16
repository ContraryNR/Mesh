#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H
#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QIODevice>
#include <QAudioSource>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudio>
#include <QMediaDevices>
#include <QThread>
#include <cmath>
#include "logger.h"

//audioSource(源/生产) => audioencoder(中继管线/消费) =待编码的PCM帧(若干采样点)=> audioEncoder

class audiocapture : public QObject
{
    Q_OBJECT
public:
    //PCMConfig
    int sampleRate;//一秒采样数
    int channelCount;//通道(声道)数
    int frameDurationMs;//单帧持续毫秒数
    int frameSizeBytes;//单帧字节数

    //RunningConfig
    bool muted{false};//本端静音停止读取 PCM(本端不再发声)
    int noiseThreshold{2};//噪声门阈值(0~100):低于此值不发送,0=关闭噪声门
    int noiseGateHold{8};//噪声门打开后保持发送的帧数(20ms/帧,8帧≈160ms),防止尾音截断
    int noiseGateHoldCounter{0};//当前剩余保持帧数
    
    //Pump(Flood)Timer
    QTimer* floodTimer{nullptr};

    QAudioSource* audioSource{nullptr};
    QIODevice* captureDevice{nullptr};

    //延迟创建参数:构造时协商好格式,在 startFloodTimer (目标线程) 上创建 QAudioSource
    QAudioDevice m_device;
    QAudioFormat m_format;

    //WebRTC/Opus推荐配置=>48kHz 单声道 帧长 20ms
    //注意:QAudioSource 必须在目标线程(trd[AU])上创建,不能在主线程创建后 moveToThread
    //因此构造函数仅保存参数,实际创建推迟到 startFloodTimer (在 trd[AU] 上执行)
    audiocapture(int sr=48000,int channelCount=1,int frameMs=20,const QAudioDevice& device=QAudioDevice(),int noiseGate=2)
        : sampleRate(sr), channelCount(channelCount), frameDurationMs(frameMs), noiseThreshold(noiseGate)
    {
        frameSizeBytes = (sampleRate / 1000) * frameDurationMs
        * channelCount * static_cast<int>(sizeof(int16_t));

        //协商格式:确定实际可用的 sr/ch
        QAudioFormat fmt;
        fmt.setSampleRate(sampleRate);
        fmt.setChannelCount(channelCount);
        fmt.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice dev = device.isNull() ? QMediaDevices::defaultAudioInput() : device;
        if(dev.isNull())
        {Logger::instance().log("[audiocapture] no default audio input device");return;}

        bool formatSupported = dev.isFormatSupported(fmt);
        if(!formatSupported)
        {
            Logger::instance().log("[audiocapture] requested format not supported, fallback to preferred");
            fmt = dev.preferredFormat();
            sampleRate = fmt.sampleRate();
            channelCount = fmt.channelCount();
            frameSizeBytes = (sampleRate / 1000) * frameDurationMs
                * channelCount * static_cast<int>(sizeof(int16_t));
        }

        //保存设备和格式供 startFloodTimer 在目标线程上使用
        m_device = dev;
        m_format = fmt;
    }

public slots:
    void startFloodTimer()
    {
        if(!audioSource)
        {
            if(m_device.isNull())
            {Logger::instance().log("[audiocapture] no device, cannot start");return;}
            audioSource = new QAudioSource(m_device, m_format, this);
            audioSource->setBufferSize(frameSizeBytes * 4);
        }

        if(audioSource->state() == QAudio::ActiveState || audioSource->state() == QAudio::IdleState)
            Logger::instance().log("[audiocapture] QAudioSource already started, skip start");
        else
        {
            captureDevice = audioSource->start();
            if(!captureDevice)
            {Logger::instance().log("[audiocapture] failed to start QAudioSource");return;}
            if(audioSource->state() != QAudio::ActiveState && audioSource->state() != QAudio::IdleState)
            {
                Logger::instance().log("[audiocapture] QAudioSource in unexpected state after start");
                return;
            }
        }

        if(!floodTimer)
        {
            floodTimer = new QTimer(this);
            connect(floodTimer, &QTimer::timeout, this, [this](){
                if(!captureDevice) return;
                if(captureDevice->bytesAvailable() < frameSizeBytes) return;
                QByteArray pcmData = captureDevice->read(frameSizeBytes);
                if(pcmData.size() == frameSizeBytes)
                {
                    const int16_t* samples = reinterpret_cast<const int16_t*>(pcmData.constData());
                    int sampleCount = frameSizeBytes / static_cast<int>(sizeof(int16_t));
                    if(sampleCount > 0)
                    {
                        double sumSq = 0.0;
                        for(int i = 0; i < sampleCount; ++i)
                        {
                            double v = static_cast<double>(samples[i]);
                            sumSq += v * v;
                        }
                        double rms = std::sqrt(sumSq / sampleCount) / 32768.0;
                        int level = static_cast<int>(rms * 100.0);
                        if(level > 100) level = 100;
                        emit sendPcmLevel(level);
                        if(noiseThreshold > 0 && level < noiseThreshold && noiseGateHoldCounter <= 0)
                            return;
                        if(level >= noiseThreshold)
                            noiseGateHoldCounter = noiseGateHold;
                        if(noiseGateHoldCounter > 0) noiseGateHoldCounter--;
                    }
                    emit sendPcmFrame(pcmData);
                }
            });
        }

        if(floodTimer && !(floodTimer->isActive()))
            floodTimer->start(frameDurationMs);
    }

    void setMuted(bool mute)
    {
        if(muted == mute)
            return;
        muted = mute;
        if(!floodTimer)
            return;
        if(muted)
        {
            if(floodTimer->isActive())
                floodTimer->stop();
            Logger::instance().log("[audiocapture] muted (floodTimer stopped)");
        }
        else
        {
            if(!floodTimer->isActive())
                floodTimer->start(frameDurationMs);
            Logger::instance().log("[audiocapture] unmuted (floodTimer restarted)");
        }
    }

    void setNoiseThreshold(int threshold, int holdFrames = 8)
    {
        noiseThreshold = qBound(0, threshold, 100);
        noiseGateHold = qMax(0, holdFrames);
    }

    void shutdown()
    {
        if(floodTimer)
        {
            floodTimer->stop();
            floodTimer->deleteLater();
            floodTimer = nullptr;
        }
        if(audioSource)
        {
            audioSource->stop();
            audioSource->deleteLater();
            audioSource = nullptr;
        }
        captureDevice = nullptr;
    }

signals:
    void sendPcmFrame(const QByteArray& pcmData);
    void sendPcmLevel(int level);
};

#endif // AUDIOCAPTURE_H
