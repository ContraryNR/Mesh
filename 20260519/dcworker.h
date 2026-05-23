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
#include <QFile>
#include <QDir>
#include "rtc/rtc.hpp"

//128*1024=131072
#define busySize 98304//*0.75(96KB)=>若最后一个包过大可能导致意外塞满
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
    QHash<QString,QHash<int,QByteArray>> fileContainer;
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
    void loadFile(const QString& fileName,int chunkIndex,int chunkAmount,const std::byte* begin,int size)
    {
        QHash<int,QByteArray>& singleFileContainer=fileContainer[fileName];
        singleFileContainer.tryEmplace(chunkIndex,(const char*)begin,size);//指定键值对(key)存在时不尝试构造value(并更新value)
        if(singleFileContainer.size()==chunkAmount)
        {
            //(循环前)只打开一次文件，循环内按顺序写入所有块
            QFile file(QDir::currentPath() + "/" + fileName);
            if (!file.open(QIODevice::WriteOnly))
            {
                qDebug()<<"文件打开失败";
                return;
            }
            for(int i=0; i<chunkAmount; i++)
            {
                const QByteArray& singlePart = singleFileContainer[i];
                qint64 bytesWritten = file.write(singlePart);
                if (bytesWritten != singlePart.size())
                {
                    qDebug()<<"文件块写入不完整";
                    file.close();
                    return;
                }
            }
            file.close();
            qDebug()<<"文件接收完成:"<<fileName;
            fileContainer.remove(fileName);//接受(写入)完成后从外层QHash移除该文件的QHash<int,QByteArray>容器
        }
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
            {//目前dc.send()0号位flag只分0/1区分tunOutBinary和MainWindowOutBinary
                QMutexLocker locker(inboundBufferMutex);
                inboundBuffer.emplace_back(binaryMsg.begin()+1,binaryMsg.end());
            }
            else
            {
                const std::byte* bp=binaryMsg.data()+1;
                uint32_t chunkIndex;
                std::memcpy(&chunkIndex,bp,4);
                bp+=4;
                uint32_t chunkAmount;
                std::memcpy(&chunkAmount,bp,4);
                bp+=4;
                uint8_t fileNameLength;
                std::memcpy(&fileNameLength,bp,1);
                bp+=1;
                std::byte* fileName=(std::byte*)malloc(fileNameLength);
                memcpy(fileName,bp,fileNameLength);
                //QString(fileName)->toUtf8()->QByteArray=本质是连续的std::byte=>接收端将连续的Utf8编码后的std::byte解码转回即可->fromUtf8()
                QString filename=QString::fromUtf8(fileName);
                bp+=fileNameLength;
                loadFile(filename,chunkIndex,chunkAmount,bp,binaryMsg.size()-(bp-binaryMsg.data()));
            }
        }
    }
public slots:
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
                     (!pendingStringMsg.isEmpty())&&(!pendingByteArrMsg.isEmpty()))
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
                isBufferBusy=true;//每次send()写入缓冲区即更新isBusy
        }
        //字符串消息应该'基本上'不需要'predict'来提前调用(毕竟本来就不多而且pending消息中也是优先处理字符串消息)
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
                outboundSpeed += msg.size();  // 统计出站速度
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
        emit dcFinish();
    }
signals:
    void dcFinish();
    void transferMsg(const QJsonObject&);
    void sendInboundSpeed(int);
    void sendOutboundSpeed(int);
    void receiveStringMsg(int peerHostNum, const QString& msg);
};
#endif
