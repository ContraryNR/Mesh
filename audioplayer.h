#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QByteArray>
#include <QIODevice>
#include <QAudioSink>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudio>
#include <QThread>
#include "logger.h"
#include <QMediaDevices>

class audioplayer : public QObject
{
    Q_OBJECT
public:
    int sampleRate;
    int channelCount;
    QAudioSink* audioSink{nullptr};
    QIODevice* playbackDevice{nullptr};

    //延迟创建参数:构造时协商好格式,在 startPlayback (目标线程) 上创建 QAudioSink
    QAudioDevice m_device;
    QAudioFormat m_format;

    //QAudioSink 必须在目标线程(trd[AU])上创建,不能在主线程创建后 moveToThread
    audioplayer(int sr=48000,int ch=1): sampleRate(sr), channelCount(ch)
    {
        QAudioFormat fmt;
        fmt.setSampleRate(sampleRate);
        fmt.setChannelCount(channelCount);
        fmt.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice dev = QMediaDevices::defaultAudioOutput();
        if(dev.isNull())
        {Logger::instance().log("[audioplayer] no default audio output device");return;}
        if(!dev.isFormatSupported(fmt))
        {
            Logger::instance().log("[audioplayer] requested format not supported, fallback to preferred");
            fmt = dev.preferredFormat();
            sampleRate = fmt.sampleRate();
            channelCount = fmt.channelCount();
        }

        m_device = dev;
        m_format = fmt;
    }

public slots:
    void startPlayback()
    {
        if(!audioSink)
        {
            if(m_device.isNull())
            {Logger::instance().log("[audioplayer] no device, cannot start");return;}
            audioSink = new QAudioSink(m_device, m_format, this);
        }

        if(audioSink->state() == QAudio::ActiveState || audioSink->state() == QAudio::IdleState)
            Logger::instance().log("[audioplayer] QAudioSink already started, skip start");
        else
        {
            playbackDevice = audioSink->start();
            if(!playbackDevice)
            {
                Logger::instance().log("[audioplayer] failed to start QAudioSink");
                return;
            }
            if(audioSink->state() != QAudio::ActiveState && audioSink->state() != QAudio::IdleState)
            {
                Logger::instance().log("[audioplayer] QAudioSink in unexpected state after start");
                playbackDevice = nullptr;
                return;
            }
        }
    }

    void playFlood(const QByteArray& pcmData)
    {
        if(!playbackDevice || pcmData.isEmpty())
            return;
        if(audioSink && audioSink->state() == QAudio::StoppedState)
            return;
        playbackDevice->write(pcmData);
    }

    void shutdown()
    {
        if(audioSink)
        {
            if(audioSink->state() != QAudio::StoppedState)
                audioSink->stop();
            audioSink->deleteLater();
            audioSink = nullptr;
        }
        playbackDevice = nullptr;
    }

signals:
    //目前没有需要主动发出去的信号,保留空位
};

#endif // AUDIOPLAYER_H
