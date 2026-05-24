#ifndef DCWORKER_H
#define DCWORKER_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <string>
#include <variant>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QMutex>
#include <QStack>
#include <QMutexLocker>
#include "rtc/rtc.hpp"
#include "filereceiver.h"

//128*1024=131072
#define busySize 104857//*0.8(102KB)
#define freeSize 32768//*0.25(32KB)=>若设置一个不够低的值可能很快又会达到busySize

class dcworker : public QObject
{
    Q_OBJECT
public://Flags
    bool isOfferER;int peerHostNum;
    std::atomic<bool> peerAlive{false},dcValid{false},isShuttingDown{false},isBufferBusy{false},newEventNow{false};
public://Sources
    std::shared_ptr<rtc::PeerConnection> pc{NULL};
    std::shared_ptr<rtc::DataChannel> dc{NULL};    
public://Speed
    std::atomic<int> outboundSpeed=0;
    QTimer* outboundSpeedTimer{NULL};
public://QTimer
    QTimer* sendTimer{NULL},*detectTimer{NULL};
    QTimer* processPendingTimer{NULL};
public://Buffer
    std::vector<rtc::binary>& inboundBuffer;
    QStack<QString> pendingStringMsg;
    QStack<QByteArray> pendingByteArrMsg;
public://Mutex
    QMutex* inboundBufferMutex{NULL};
public://fileReceiver
    filereceiver* fr{nullptr};
    QThread* frTrd{nullptr};
    dcworker(bool identity,int peerNum,std::vector<rtc::binary>& inBuffer,QMutex* mtx):isOfferER(identity),peerHostNum(peerNum),inboundBuffer(inBuffer),inboundBufferMutex(mtx)
    {
        processPendingTimer=new QTimer(this);
        processPendingTimer->setInterval(100);
        connect(processPendingTimer,&QTimer::timeout,this,&dcworker::processPenddingMsgStack);
        outboundSpeedTimer = new QTimer(this);
        outboundSpeedTimer->setInterval(1000);
        connect(outboundSpeedTimer, &QTimer::timeout, this, [this](){
            emit sendOutboundSpeed(outboundSpeed);
            outboundSpeed = 0;
        });
    }
public://toolFunction
    QJsonObject offerPacker(const QString& sdp)
    {
        QJsonObject offer;
        offer["type"]="sdp";
        offer["sdpType"]="offer";
        offer["sdp"]=sdp;
        offer["target"]=peerHostNum;
        return offer;
    }
    QJsonObject answerPacker(const QString& sdp)
    {
        QJsonObject answer;
        answer["type"]="sdp";
        answer["sdpType"]="answer";
        answer["sdp"]=sdp;
        answer["target"]=peerHostNum;
        return answer;
    }
    QJsonObject candidatePacker(const QString& candidate,const QString& mid)
    {
        QJsonObject candidateJson;
        candidateJson["type"]="candidate";
        candidateJson["candidateItem"]=candidate;
        candidateJson["candidateMid"]=mid;
        candidateJson["target"]=peerHostNum;
        return candidateJson;
    }
    void onDcMsg(std::variant<rtc::binary, rtc::string> message)
    {
        peerAlive.store(true, std::memory_order_relaxed);
        if (std::holds_alternative<std::string>(message))
        {
            QString msg=QString::fromStdString(std::get<std::string>(message));
            if(msg!=QString("hb")) {
                emit receiveStringMsg(peerHostNum, msg);
            }
        }
        else
        {
            std::vector<std::byte> binaryMsg = std::get<rtc::binary>(message);
            int size=binaryMsg.size();
            if (!size) return;
            emit sendInboundSpeed(size);
            if(!(uint8_t)binaryMsg[0])
            {
                QMutexLocker locker(inboundBufferMutex);
                inboundBuffer.emplace_back(binaryMsg.begin()+1,binaryMsg.end());
            }
            else
            {
                if(!fr)//使用跨线程阻塞等待 确保QThread线程归属正确的同时避免异步执行事件太晚导致将byteArray(chunk)投递到nullptr
                    QMetaObject::invokeMethod(this,"startFileReceiver",Qt::BlockingQueuedConnection);
                const std::byte* bp=binaryMsg.data()+1;
                uint32_t chunkIndex;
                std::memcpy(&chunkIndex,bp,4);bp+=4;
                uint32_t chunkAmount;
                std::memcpy(&chunkAmount,bp,4);bp+=4;                
                uint8_t fileNameLength;
                std::memcpy(&fileNameLength,bp,1);bp+=1;
                std::byte* filename=(std::byte*)malloc(fileNameLength);
                std::memcpy(filename,bp,fileNameLength);bp+=fileNameLength;
                QString fileName=QString::fromUtf8((const char*)filename,fileNameLength);

                // // 添加调试日志 - 显示文件名原始字节
                // QByteArray fileNameBytes((const char*)filename,fileNameLength);
                // qDebug() << "解析文件块:" << fileName << "块索引:" << chunkIndex << "总块数:" << chunkAmount
                //          << "文件名长度:" << (int)fileNameLength << "文件名字节:" << fileNameBytes.toHex()
                //          << "数据大小:" << (binaryMsg.size() - (bp - binaryMsg.data()));
                
                emit goReceiveFile(fileName,chunkIndex,chunkAmount,QByteArray(
                (const char*)bp,binaryMsg.size()-(bp-binaryMsg.data())));
                free(filename);
            }
        }
    }
public slots:
    void startFileReceiver()
    {
        (fr=new filereceiver)->moveToThread(frTrd=new QThread);
        frTrd->start();fr->running=true;
        connect(this,&dcworker::goReceiveFile,fr,&filereceiver::receiveFile);
    }
    void processPenddingMsgStack()
    {
        newEventNow=false;//仅负责timerSlot执行期间的事件检查
        do//每次调用必然至少取出一条消息并发送
        {
            if(!isBufferBusy&&dc&&dc->isOpen()&&dcValid)
            {
                try
                {
                    if(!pendingStringMsg.isEmpty())
                    {
                        dc->send(pendingStringMsg.top().toStdString());
                        pendingStringMsg.pop();//先处理的总是最近(积压)的消息
                    }
                    else
                    {
                        if(!pendingByteArrMsg.isEmpty())//仅当确定没有stringMsg(聊天消息)时才处理binaryMsg
                        {
                            dc->send((const rtc::byte*)(pendingByteArrMsg.top().data()),pendingByteArrMsg.top().size());
                            pendingByteArrMsg.pop();
                        }
                    }
                } catch (const std::exception& e)
                {
                    qWarning() << "dc send failed:" << e.what();
                    dcValid = false;
                }
                //单次发送后更新isBusy状态
                if(dc->bufferedAmount()>busySize)
                    isBufferBusy=true;
            }
        }
        while(!newEventNow&&
                (!isBufferBusy)&&
                 (
                     (!pendingStringMsg.isEmpty())||(!pendingByteArrMsg.isEmpty()))
                     //注意不要把"||"写反成"&&",否则两个pendingStack只要有一个空循环都只能跑一次
                 );
    }
    //注/虽然划分为两个sender函数 但是不需要互斥锁 因为异步投递的event最终只能同步执行
    void sendStringMsg(const QString& Msg)
    {
        if(isBufferBusy)
            pendingStringMsg.push(Msg);
        else if(dc&&dc->isOpen()&&dcValid)
        {
            try
            {
                dc->send(Msg.toStdString());
            } catch (const std::exception& e)
            {
                qWarning() << "dc string send failed:" << e.what();
                dcValid = false;
            }
            if(dc->bufferedAmount()>busySize)
                isBufferBusy=true;
        }
    }
    void sendBinaryMsg(const QByteArray& msg,bool predictNextEvent)
    {
        if(isBufferBusy)
            pendingByteArrMsg.push(msg);
        else if(dc&&dc->isOpen()&&dcValid)
        {
            try
            {
                dc->send((const rtc::byte*)msg.data(), msg.size());
                outboundSpeed += msg.size();
            } catch (const std::exception& e)
            {
                qWarning() << "dc binary send failed:" << e.what();
                dcValid = false;
            }
            if(dc->bufferedAmount()>busySize)
                isBufferBusy=true;
        }
        if(!predictNextEvent)
            processPenddingMsgStack();
    }
    void createDc()
    {
        rtc::Configuration config;
        config.iceServers.emplace_back("stun:stun.l.google.com:19302");
        pc= std::make_shared<rtc::PeerConnection>(config);
        pc->onLocalDescription([this](rtc::Description sdp) {
            if(isOfferER)
                emit transferMsg(offerPacker(QString::fromStdString(std::string(sdp))));
            else
                emit transferMsg(answerPacker(QString::fromStdString(std::string(sdp))));
        });
        pc->onLocalCandidate([this](rtc::Candidate candidate) {
            emit transferMsg(candidatePacker(QString::fromStdString(std::string(candidate)),QString::fromStdString(candidate.mid())));
        });
        if(isOfferER)
        {
            dc=pc->createDataChannel("P2PConnection");
            dc->onOpen([this](){
                QMetaObject::invokeMethod(this,"vade",Qt::QueuedConnection);
                dcValid=true;
                dc->setBufferedAmountLowThreshold(freeSize);
                dc->onBufferedAmountLow([this]() {
                    isBufferBusy=false;
                });
                dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                    onDcMsg(message);
                });
                QMetaObject::invokeMethod(this,"startProcessPendingStackTimer",Qt::QueuedConnection);
                QMetaObject::invokeMethod(this,"startOutBoundSpeedTimer",Qt::QueuedConnection);
                dc->onClosed([this](){
                    dcValid=false;
                    QMetaObject::invokeMethod(this, "shutdown", Qt::QueuedConnection);
                });
            });
        }
        else
        {
            pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> incoming) {
                dc=incoming;
                dc->onOpen([this](){
                    QMetaObject::invokeMethod(this,"vade",Qt::QueuedConnection);
                    dcValid=true;
                    dc->setBufferedAmountLowThreshold(freeSize);
                    dc->onBufferedAmountLow([this]() {//dc缓冲区从繁忙状态(高占用)将降到低占用时触发回调
                        isBufferBusy=false;
                    });
                    dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                        onDcMsg(message);
                    });
                    QMetaObject::invokeMethod(this,"startProcessPendingStackTimer",Qt::QueuedConnection);
                    QMetaObject::invokeMethod(this,"startOutBoundSpeedTimer",Qt::QueuedConnection);
                    dc->onClosed([this](){
                        dcValid=false;
                        QMetaObject::invokeMethod(this, "shutdown", Qt::QueuedConnection);
                    });
                });
            });
        }
    }
    void setRemoteSdp(const QString& sdp,const QString& sdpType) {
        if (pc && !sdp.isEmpty())
        {
            auto currentState = pc->signalingState();
            if((sdpType == "answer" && currentState == rtc::PeerConnection::SignalingState::HaveLocalOffer)
                ||(
                sdpType == "offer" && currentState == rtc::PeerConnection::SignalingState::Stable))
            {
                rtc::Description description=rtc::Description(sdp.toStdString(),((sdpType == "offer") ?rtc::Description::Type::Offer:rtc::Description::Type::Answer));
                pc->setRemoteDescription(description);
                if(description.type()==rtc::Description::Type::Offer)
                    pc->setLocalDescription();
            }
        }
    }
    void receiveCandidate(const QString &sdp, const QString &mediaType){
        if(pc&&!sdp.isNull()&&!mediaType.isNull())
            pc->addRemoteCandidate(rtc::Candidate(sdp.toStdString(), mediaType.toStdString()));
    };
    void vade()
    {
        if(dc&&dc->isOpen())
        {
            sendTimer=new QTimer(this);
            sendTimer->setInterval(1500);
            connect(sendTimer,&QTimer::timeout,[this](){
                sendStringMsg("hb");
            });
            sendTimer->start();
            detectTimer=new QTimer(this);
            detectTimer->setInterval(3000);
            connect(detectTimer,&QTimer::timeout,[this](){
                if(!peerAlive)
                {
                    dcValid=false;
                    dc->onMessage(nullptr);
                    sendTimer->stop();
                    detectTimer->stop();
                    shutdown();
                }
                else
                    peerAlive=false;
            });
            detectTimer->start();
        }
    }
    void startProcessPendingStackTimer()
    {
        processPendingTimer->start();
    }
    void startOutBoundSpeedTimer()
    {
        outboundSpeedTimer->start();
    }
    void shutdown()
    {
        if (isShuttingDown.exchange(true)) return;
        if(fr)
            fr->running=false;
        if(frTrd)
            frTrd->quit();
        if(sendTimer)
        {
            if(sendTimer->isActive())
                sendTimer->stop();
            sendTimer->deleteLater();
            sendTimer=nullptr;
        }
        if(detectTimer)
        {
            if(detectTimer->isActive())
                detectTimer->stop();
            detectTimer->deleteLater();
            detectTimer=nullptr;
        }
        if(processPendingTimer)
        {
            if(processPendingTimer->isActive())
                processPendingTimer->stop();
            processPendingTimer->deleteLater();
            processPendingTimer=nullptr;
        }
        if(outboundSpeedTimer)
        {
            if(outboundSpeedTimer->isActive())
                outboundSpeedTimer->stop();
            outboundSpeedTimer->deleteLater();
            outboundSpeedTimer=nullptr;
        }
        if(dc)
        {
            dc->onMessage(nullptr);
            dc->onOpen(nullptr);
            dc->onClosed(nullptr);
        }
        if (dc)
        {
            dc->close();
            dc.reset();
        }
        if (pc)
        {
            pc->close();
            pc.reset();
        }
        if(frTrd)
        {
            frTrd->wait();
            delete(frTrd);
            frTrd=nullptr;
            delete(fr);fr=nullptr;
        }
        emit dcFinish();
    }
signals:
    void dcFinish();
    void transferMsg(const QJsonObject&);
    void sendInboundSpeed(int);
    void sendOutboundSpeed(int);
    void receiveStringMsg(int peerHostNum, const QString& msg);
    void goReceiveFile(const QString& fileName,int chunkIndex,int chunkAmount,const QByteArray& chunk);
};
#endif
