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
#include "filedownloader.h"

class fileDownLoadState
{
public:
    QString filename;
    int peerHostNum;
    qreal progress;
    fileDownLoadState(){}
    fileDownLoadState(QString filename, int peerHostNum, qreal progress)
        : filename(std::move(filename)), peerHostNum(peerHostNum),
        progress(progress) {}
    fileDownLoadState(const fileDownLoadState& oldOne)
        :filename(oldOne.filename), peerHostNum(oldOne.peerHostNum),
        progress(oldOne.progress) {}
};

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
    QTimer* basicTimer{NULL};
public://QTimer
    QTimer* sendTimer{NULL},*detectTimer{NULL};
    QTimer* processPendingTimer{NULL};
public://Buffer
    std::vector<rtc::binary>& inboundBuffer;
    QStack<QString> pendingStringMsg;
    QStack<QByteArray> pendingByteArrMsg;
public://Mutex
    QMutex* inboundBufferMutex{NULL};
public://fileDwnloader
    QHash<QString,QPair<filedownloader*,QThread*>> fileContainer;
public://Setting
    std::atomic<int> busySize=104857;
    std::atomic<int> freeSize=32768;
    dcworker(bool identity,int peerNum,std::vector<rtc::binary>& inBuffer,QMutex* mtx,int bSize=104857,int fSize=32768):isOfferER(identity),peerHostNum(peerNum),inboundBuffer(inBuffer),inboundBufferMutex(mtx),busySize(bSize),freeSize(fSize)
    {
        processPendingTimer=new QTimer(this);
        processPendingTimer->setInterval(100);
        connect(processPendingTimer,&QTimer::timeout,this,&dcworker::processPenddingMsgStack);
        basicTimer = new QTimer(this);
        basicTimer->setInterval(1000);
        connect(basicTimer, &QTimer::timeout, this, [this](){
            emit sendOutboundSpeed(outboundSpeed);
            outboundSpeed = 0;
            QList<fileDownLoadState> fileState;
            for(auto& pr:fileContainer)
                fileState.emplace_back(pr.first->fileName,peerHostNum,(qreal)(pr.first->writedChunkAmount)/(qreal)(pr.first->totalChunkAmount)*100);
            if(!fileState.isEmpty())
                emit informFileDownLoadState(fileState);
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
                const std::byte* bp=binaryMsg.data()+1;
                uint64_t chunkIndex;
                std::memcpy(&chunkIndex,bp,8);bp+=8;
                uint64_t chunkAmount;
                std::memcpy(&chunkAmount,bp,8);bp+=8;              
                uint8_t fileNameLength;
                std::memcpy(&fileNameLength,bp,1);bp+=1;
                std::byte* filename=(std::byte*)malloc(fileNameLength);
                std::memcpy(filename,bp,fileNameLength);bp+=fileNameLength;
                QString fileName=QString::fromUtf8((const char*)filename,fileNameLength);

                if(!fileContainer.contains(fileName))
                    QMetaObject::invokeMethod(this,"startSingleFileDownLoader",Qt::BlockingQueuedConnection,Q_ARG(const QString&,fileName),Q_ARG(uint64_t,chunkAmount));
                QMetaObject::invokeMethod(fileContainer[fileName].first,"writeToChunkIndex",Qt::QueuedConnection,
                                              Q_ARG(uint64_t,chunkIndex),
                                              Q_ARG(const QByteArray&,QByteArray(
                                                                            (const char*)bp,binaryMsg.size()-(bp-binaryMsg.data())
                                                                            ))) ;
                free(filename);
            }
        }
    }
public slots:
    void startSingleFileDownLoader(const QString& fileName,uint64_t chunkAmount)
    {
        filedownloader* worker;QThread* trd;
        fileContainer.insert(fileName,QPair<filedownloader*,QThread*>(worker=new filedownloader(fileName,chunkAmount),trd=new QThread));
        connect(trd,&QThread::finished,trd,&QThread::deleteLater);
        connect(worker,&filedownloader::fileWriteFinished,this,[this](const QString& filename){
            emit informFileDownLoadFinish(filename,peerHostNum);
            fileContainer[filename].second->quit();
            fileContainer.remove(filename);
        });
        worker->moveToThread(trd);
        trd->start();
        worker->running = true;
    }
    void processPenddingMsgStack()
    {
        newEventNow=false;
        int binaryPktSize=0;
        do
        {
            if(!isBufferBusy&&dc&&dc->isOpen()&&dcValid)
            {
                try
                {
                    if(!pendingStringMsg.isEmpty())
                    {
                        dc->send(pendingStringMsg.top().toStdString());
                        pendingStringMsg.pop();
                    }
                    else
                    {
                        if(!pendingByteArrMsg.isEmpty())
                        {
                            dc->send((const rtc::byte*)(pendingByteArrMsg.top().data()),binaryPktSize=pendingByteArrMsg.top().size());
                            outboundSpeed += binaryPktSize;
                            pendingByteArrMsg.pop();
                        }
                    }
                } catch (const std::exception& e)
                {
                    qWarning() << "dc send failed:" << e.what();
                    dcValid = false;
                }
                if(dc->bufferedAmount()>busySize)
                    isBufferBusy=true;
            }
        }
        while(!newEventNow&&(!isBufferBusy)&&((!pendingStringMsg.isEmpty())||(!pendingByteArrMsg.isEmpty())));
    }
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
    void updateSettings(int bSize, int fSize)
    {
        busySize = bSize;
        freeSize = fSize;
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
        basicTimer->start();
    }
    void shutdown()
    {
        if (isShuttingDown.exchange(true)) return;
        if(!fileContainer.isEmpty())
            for(auto& pr:fileContainer.values())
            {
                pr.first->running=false;
                pr.first->deleteLater();
                pr.second->quit();
                pr.second->wait();
            }
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
        if(basicTimer)
        {
            if(basicTimer->isActive())
                basicTimer->stop();
            basicTimer->deleteLater();
            basicTimer=nullptr;
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
    void informFileDownLoadFinish(const QString& filename,int peerHostNum);
    void informFileDownLoadState(const QList<fileDownLoadState>&);
};
#endif
