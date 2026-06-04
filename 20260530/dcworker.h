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
#include <QQueue>
#include <QMutexLocker>
#include "rtc/rtc.hpp"
#include "filedownloader.h"
#include "videodecoder.h"
#include "logger.h"

class fileDownLoadState
{
public:
    QString filename;
    int peerHostNum;
    qreal progress=0;
    fileDownLoadState(){}
    fileDownLoadState(QString filename, int peerHostNum)
        : filename(std::move(filename)), peerHostNum(peerHostNum){}
    fileDownLoadState(QString filename, int peerHostNum, qreal progress)
        : filename(std::move(filename)), peerHostNum(peerHostNum),
        progress(progress) {}
    fileDownLoadState(const fileDownLoadState& oldOne)
        :filename(oldOne.filename), peerHostNum(oldOne.peerHostNum),
        progress(oldOne.progress) {}
};

class workerState
{
public:
    int inBoundSpeed;
    int outBoundSpeed;
    int pressure;
    QList<fileDownLoadState> fileState;
    workerState(){}
    workerState(int ibs, int obs, int pr)
        : inBoundSpeed(ibs), outBoundSpeed(obs),
        pressure(pr) {}
    workerState(const workerState& oldOne)
        : inBoundSpeed(oldOne.inBoundSpeed), outBoundSpeed(oldOne.outBoundSpeed),
        pressure(oldOne.pressure) {}
    void reset()
    {inBoundSpeed=outBoundSpeed=pressure=0;}
};

class dcworker : public QObject
{
    Q_OBJECT
public://Flags
    std::atomic<bool> peerAlive{false},dcValid{false},
    isShuttingDown{false},isBufferBusy{false},newEventNow{false},isVideoCalling{false};
    bool isOfferER;int peerHostNum,index;
    workerState state;
    workerState getState()
    {
        workerState cpy=state;
        state.reset();
        return cpy;
    }

public://timePointFlag
    std::chrono::time_point<std::chrono::steady_clock> lastUpdateTime;

public://Sources
    std::shared_ptr<rtc::PeerConnection> pc{NULL};
    std::shared_ptr<rtc::DataChannel> dc{NULL};

public://QTimer
    QTimer* sendTimer{NULL},*detectTimer{NULL};
    QTimer* processPendingTimer{NULL};

public://Buffer
    std::vector<rtc::binary>& inboundBuffer;
    QStack<QByteArray>* pendingTun{nullptr};
    QQueue<QString>* pendingString{nullptr};
    QQueue<QByteArray>* pendingFile{nullptr};
    QQueue<QByteArray>* pendingFrame{nullptr};
    QQueue<QByteArray>* pendingAudio{nullptr};
    QByteArray* tempByteArrayPtr{nullptr};

public://Mutex
    QMutex* inboundBufferMutex{NULL};

public://fileDwnloader
    QHash<QString,QPair<filedownloader*,QThread*>> fileContainer;

public://Setting
    std::atomic<int> busySize=104857;
    std::atomic<int> freeSize=32768;

public://frameDecoder
    videodecoder* deCoder{nullptr};
    QThread* deCoderTrd{nullptr};

public://constructorFunction
    dcworker(bool identity,int peerNum,int vecIndex,std::vector<rtc::binary>& inBuffer,QMutex* mtx,int bSize=104857,int fSize=32768)
        :isOfferER(identity),peerHostNum(peerNum),inboundBuffer(inBuffer),inboundBufferMutex(mtx),
        busySize(bSize),freeSize(fSize),
        tempByteArrayPtr(new QByteArray())
    {
        lastUpdateTime=std::chrono::steady_clock::now();
        index=vecIndex;
        switch(index)
        {
            case 0://Main(tun/string)(/audio)
            {
                pendingTun=new QStack<QByteArray>;
                pendingString=new QQueue<QString>;
                pendingFile=new QQueue<QByteArray>;
                pendingAudio=new QQueue<QByteArray>;
                break;
            }
            case 1://file
            {
                pendingFile=new QQueue<QByteArray>;
                break;
            }
            case 2://audio
            {
                pendingAudio=new QQueue<QByteArray>;
                break;
            }
            case 3://video
            {
                pendingFrame=new QQueue<QByteArray>;
                break;
            }
        }
        processPendingTimer=new QTimer(this);
        processPendingTimer->setInterval(100);
        connect(processPendingTimer,&QTimer::timeout,this,&dcworker::processPenddingMsg);
    }

public://toolFunction
    QJsonObject offerPacker(const QString& sdp)
    {
        QJsonObject offer;
        offer["type"]="sdp";
        offer["sdpType"]="offer";
        offer["sdp"]=sdp;
        offer["target"]=peerHostNum;
        offer["index"]=index;
        return offer;
    }
    QJsonObject answerPacker(const QString& sdp)
    {
        QJsonObject answer;
        answer["type"]="sdp";
        answer["sdpType"]="answer";
        answer["sdp"]=sdp;
        answer["target"]=peerHostNum;
        answer["index"]=index;
        return answer;
    }
    QJsonObject candidatePacker(const QString& candidate,const QString& mid)
    {
        QJsonObject candidateJson;
        candidateJson["type"]="candidate";
        candidateJson["candidateItem"]=candidate;
        candidateJson["candidateMid"]=mid;
        candidateJson["target"]=peerHostNum;
        candidateJson["index"]=index;
        return candidateJson;
    }

public://dc.OnMsg.CALLBACK
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
            state.inBoundSpeed += size;
            const std::byte* bp=binaryMsg.data();
            switch((uint8_t)(*(bp++)))
            {
                case 0://TUN
                {
                    QMutexLocker locker(inboundBufferMutex);
                    inboundBuffer.emplace_back(binaryMsg.begin()+1,binaryMsg.end());
                    break;
                }
                case 1://NEGOTIATE
                {
                    //flag=1 NEGOTIATE类型前缀

                    uint8_t neogotiateFlag;
                    //0file/2audio/3video
                    //后续用于确认调用哪个函数来返回信息
                    //(例如如果是文件请求就用sendFileMsg返回结果)
                    std::memcpy(&neogotiateFlag,bp,1);bp+=1;

                    uint8_t neogotiateSate;
                    std::memcpy(&neogotiateSate,bp,1);bp+=1;
                    //0拒绝/1同意/10询问/20中断

                    uint64_t requestTime;
                    std::memcpy(&requestTime,bp,8);bp+=8;
                    //requestTimePoint(毫秒级时间戳)唯一标识request

                    if(neogotiateSate==10)
                    {
                        QString explain;
                        int remainLength=binaryMsg.size()-(bp-binaryMsg.data());
                        if(remainLength>0)//仅'询问'消息携带补充说明
                            explain=QString::fromUtf8((const char*)bp,remainLength);
                        emit transferRequest(neogotiateFlag,requestTime,explain,this);
                    }
                    else if(neogotiateSate==20)
                    {
                        isVideoCalling=false;
                        emit videoHangupReceived(peerHostNum);
                    }
                    else
                        emit returnRequestResult(requestTime,neogotiateSate);
                    break;
                }
                case 2://FILE
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
                    {
                        QMetaObject::invokeMethod(this,"startSingleFileDownLoader",Qt::BlockingQueuedConnection,Q_ARG(const QString&,fileName),Q_ARG(uint64_t,chunkAmount));
                        state.fileState.emplace_back(fileName,peerHostNum);
                    }
                    QMetaObject::invokeMethod(fileContainer[fileName].first,"writeToChunkIndex",Qt::QueuedConnection,
                                              Q_ARG(uint64_t,chunkIndex),
                                              Q_ARG(const QByteArray&,QByteArray(
                                                                            (const char*)bp,binaryMsg.size()-(bp-binaryMsg.data())
                                                                            ))) ;
                    free(filename);
                    break;
                }
                case 3://AUDIO
                {

                    break;
                }
                case 4://VIDEO
                {
                    if(!isShuttingDown && isVideoCalling)
                    {
                        if(!deCoder)
                            QMetaObject::invokeMethod(this,"startDeCoder",Qt::BlockingQueuedConnection);
                        int bytes=binaryMsg.size()-1;
                        std::byte* singleFrameData=(std::byte*)malloc(bytes);
                        std::memcpy(singleFrameData,bp,bytes);
                        QMetaObject::invokeMethod(deCoder,"decodedFlood",Qt::QueuedConnection,Q_ARG(void*,(void*)singleFrameData),Q_ARG(int,bytes));
                    }
                    break;
                }
            }
        }
    }

public slots://startDeCoderSlot
    void startDeCoder()
    {
        (deCoder=new videodecoder)->moveToThread(deCoderTrd=new QThread);
        connect(deCoder,&videodecoder::sendDecodedFrame,this,[this](const QImage& frameImg){
            emit transferDecodedFrame(frameImg,peerHostNum);
        });
        deCoderTrd->start();
    }

public slots://startFileDownLoadSlot
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
        connect(worker,&filedownloader::fileWriteProgress,this,[this](const QString& fileName,int progress){
            QList<fileDownLoadState>& stateList=state.fileState;
            for(auto beg=stateList.begin();beg!=stateList.end();beg++)
                if(beg->filename==fileName)
                {
                    beg->progress=progress;
                    break;
                }
        });
        worker->moveToThread(trd);
        trd->start();
        worker->running = true;
    }

public://sendFunctionRefaction
    void sendMsg(const QString& Msg)
    {
        if(dc&&dc->isOpen()&&dcValid)
        {
            try
            {
                dc->send(Msg.toStdString());
                if(dc->bufferedAmount()>busySize)
                    isBufferBusy=true;
            } catch (const std::exception& e)
            {
                qWarning() << "dc string send failed:" << e.what();
                dcValid = false;
            }
        }
    }
    void sendMsg(const QByteArray& Msg)
    {
        if((std::chrono::duration_cast<std::chrono::milliseconds>(lastUpdateTime-std::chrono::steady_clock::now())).count()>=900)
        {
            if(index==0)
                state.pressure=pendingTun->size()+pendingFile->size()+pendingFrame->size();
            else if(index==2)
                state.pressure=pendingFile->size();
            lastUpdateTime=std::chrono::steady_clock::now();
        }
        if(dc&&dc->isOpen()&&dcValid)
        {
            try
            {
                dc->send((const rtc::byte*)Msg.data(), Msg.size());
                state.outBoundSpeed += Msg.size();
                if(dc->bufferedAmount()>busySize)
                    isBufferBusy=true;
            } catch (const std::exception& e)
            {
                qWarning() << "dc binary send failed:" << e.what();
                dcValid = false;
            }
        }
    }

public://pendingDataFetcher
    bool getBinaryMsg()
    {//固定优先级: video > tun > file
        if(pendingFrame&&!pendingFrame->isEmpty())
        {
            *tempByteArrayPtr=pendingFrame->dequeue();
            return true;
        }
        if(pendingTun&&!pendingTun->isEmpty())
        {
            *tempByteArrayPtr=pendingTun->top();
            pendingTun->pop();
            return true;
        }
        if(pendingFile&&!pendingFile->isEmpty())
        {
            *tempByteArrayPtr=pendingFile->dequeue();
            return true;
        }
        return false;
    }

public slots://sendSlot
    void sendStringMsg(const QString& Msg)
    {
        if(isBufferBusy&&pendingString)
            pendingString->enqueue(Msg);
        else
            sendMsg(Msg);
    }
    void sendTunMsg(const QByteArray& Msg,bool predictNextEvent)
    {
        if(isBufferBusy)
            pendingTun->push(Msg);
        else
            sendMsg(Msg);
        if(!predictNextEvent)
            processPenddingMsg();
    }
    void sendFileMsg(const QByteArray& Msg,bool predictNextEvent)
    {
        if(isBufferBusy)
        {
            if(index==1)
                pendingFile->enqueue(Msg);
            else if(index==0)
                pendingTun->push(Msg);
        }
        else
            sendMsg(Msg);
        if (!predictNextEvent)
            processPenddingMsg();
    }
    void sendVideoMsg(const QByteArray& frame)
    {
        if(isBufferBusy)
            pendingFrame->enqueue(frame);
        else
            sendMsg(frame);
    }
    void sendAudioMsg(const QByteArray& audio)
    {
        if(isBufferBusy)
            pendingAudio->enqueue(audio);
        else
            sendMsg(audio);
    }
    void processPenddingMsg()
    {
        newEventNow=false;
        int binaryPktSize=0;
        bool remainBinaryMsg=false;
        do
        {
            if(!isBufferBusy)
                if(dc&&dc->isOpen()&&dcValid)
                {
                    if(pendingString&&!pendingString->isEmpty())
                        sendMsg(pendingString->dequeue());
                    else
                    {
                        remainBinaryMsg=getBinaryMsg();
                        sendMsg(*tempByteArrayPtr);
                    }
                }
        }
        while(!newEventNow&&!isBufferBusy&&remainBinaryMsg);
    }

public slots://timerSlot
    void startProcessPendingStackTimer()
    {processPendingTimer->start();}

public slots://bootSlot
    void createDc()
    {
        rtc::Configuration config;
        config.iceServers.emplace_back("stun:stun.l.google.com:19302");
        pc= std::make_shared<rtc::PeerConnection>(config);
        pc->onLocalDescription([this](rtc::Description sdp) {
            if(isOfferER)
                emit sendSignalingMsg(offerPacker(QString::fromStdString(std::string(sdp))));
            else
                emit sendSignalingMsg(answerPacker(QString::fromStdString(std::string(sdp))));
        });
        pc->onLocalCandidate([this](rtc::Candidate candidate) {
            emit sendSignalingMsg(candidatePacker(QString::fromStdString(std::string(candidate)),QString::fromStdString(candidate.mid())));
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
                    dc->onBufferedAmountLow([this](){
                        isBufferBusy=false;
                    });
                    dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                        onDcMsg(message);
                    });
                    QMetaObject::invokeMethod(this,"startProcessPendingStackTimer",Qt::QueuedConnection);
                    dc->onClosed([this](){
                        dcValid=false;
                        QMetaObject::invokeMethod(this, "shutdown", Qt::QueuedConnection);
                    });
                });
            });
        }
    }

public slots://runningTimeSlot
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
                if(description.type()==rtc::Description::Type::Offer) {
                    pc->setLocalDescription();
                }
            } else {
                qWarning() << "[DC] State check FAILED!";
            }
        } else {
            qWarning() << "[DC] setRemoteSdp: pc is null or sdp is empty!";
        }
    }
    void receiveCandidate(const QString &sdp, const QString &mediaType){
        if(pc&&!sdp.isNull()&&!mediaType.isNull()) {
            pc->addRemoteCandidate(rtc::Candidate(sdp.toStdString(), mediaType.toStdString()));
        } else {
            qWarning() << "[DC] receiveCandidate: pc is null or params are null!";
        }
    };

public slots://heartbeatSlot
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

public slots://shutSlot
    void shutdown()
    {
        if (isShuttingDown.exchange(true)) return;
        if(deCoder)
        {
            deCoder->deleteLater();
            deCoderTrd->quit();
            deCoderTrd->wait();
            deCoderTrd->deleteLater();
            deCoder=nullptr;
            deCoderTrd=nullptr;
        }
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

public slots://settingSlot
    void updateSettings(int bSize, int fSize)
    {busySize = bSize;
    freeSize = fSize;}
signals:
    void dcFinish();
    void sendSignalingMsg(const QJsonObject&);
    void receiveStringMsg(int peerHostNum, const QString& msg);
    void informFileDownLoadFinish(const QString& filename,int peerHostNum);
    void transferDecodedFrame(const QImage&,int);
    void transferRequest(uint8_t requestFlag,uint64_t requestTime,const QString& explain,void* voidDCWorker);
    void returnRequestResult(uint64_t requestTime,bool result);
    void videoHangupReceived(int peerHostNum);
};
#endif
