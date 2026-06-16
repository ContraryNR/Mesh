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
#include "audiodecoder.h"
#include "logger.h"

//全局类型标识(统一用于: 二进制消息协议首字节 / worker索引 / purpose参数 / 协商子类型)
#define TYPE_NEGOTIATE  0   //协商(文件/音频/视频请求与响应)
#define TYPE_TUN        1   //TUN隧道数据
#define TYPE_FILE       2   //文件分块传输
#define TYPE_AUDIO      3   //音频(上游暂未实现)
#define TYPE_VIDEO      4   //视频帧(JPEG编码)
#define TYPE_JSON       5

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
        pressure(oldOne.pressure), fileState(oldOne.fileState) {}
    //不要漏了内部fileDownloadStateList的拷贝构造参数 否则传到MainWindow时为空
    void reset()
    {inBoundSpeed=outBoundSpeed=pressure=0;}
};

//(1)音视频通话的协商参数(由 mainwindow::onTransferRequest accept 后直接写 worker 的成员;
//   receiver 端真正开始传输帧时,startAudioDeCoder 用这些参数构造 decoder;
//   sender 端在 confirmStart* 时把本地栈上的这些参数传给 encoder 构造函数)
//(2)audio 通话只填 audioSampleRate/audioChannelCount,video 通话会同时填 videoWidth/Height/Fps 和音频
//(3)0 表示"未协商",由 startAudioDeCoder fallback 到 audio 默认 48000Hz/1ch
//(4)所有字段都是 std::atomic<int>,确保 mainwindow 线程直接写入 worker 成员是安全的

class dcworker : public QObject
{
    Q_OBJECT
public://Flags
    std::atomic<bool> peerAlive{false},dcValid{false},
    isShuttingDown{false},isBufferBusy{false},newEventNow{false},isVideoCalling{false},isAudioCalling{false};
    bool isOfferER,remoteDescSet{false};int peerHostNum,index;
    QVector<QPair<QString,QString>> pendingCandidates;
    workerState state;
    workerState getState()
    {
        workerState cpy=state;
        state.reset();
        return cpy;
    }

public://CallNegotiateParams
    //音频格式由 audioMsg header 携带,dcworker 在收到首帧时创建 decoder
    //若通话期间格式变化(理论上不会),dcworker 负责重建 decoder
    int audioCallSampleRate{0};
    int audioCallChannelCount{0};

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

public://videoDecoder
    videodecoder* deCoder{nullptr};
    QThread* deCoderTrd{nullptr};

public://audioDecoder
    audiodecoder* audioDeCoder{nullptr};
    QThread* audioDeCoderTrd{nullptr};

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
            case 0://主通道(tun/string/协商)
            {
                pendingTun=new QStack<QByteArray>;
                pendingString=new QQueue<QString>;
                pendingFile=new QQueue<QByteArray>;
                pendingAudio=new QQueue<QByteArray>;
                break;
            }
            case 1://文件传输
            {
                pendingFile=new QQueue<QByteArray>;
                break;
            }
            case 2://音频(暂未实现)
            {
                pendingAudio=new QQueue<QByteArray>;
                break;
            }
            case 3://视频通话
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
        offer["initialOffer"]=((index==0)?1:0);
        //不要用bool 增加'迷惑性' 避免误以为是字符串
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
                case TYPE_NEGOTIATE://协商(请求/响应)
                {
                    uint8_t msgType;
                    //TYPE_FILE/TYPE_AUDIO/TYPE_VIDEO
                    //后续用于确认调用哪个函数来返回信息
                    //(例如如果是文件请求就用sendFileMsg返回结果)
                    std::memcpy(&msgType,bp,1);bp+=1;

                    uint8_t neogotiateSate;
                    std::memcpy(&neogotiateSate,bp,1);bp+=1;
                    //0拒绝/1同意(response) / 10询问(request) / 20中断(hangup)

                    if(neogotiateSate==10)
                    {
                        //(1)wire 格式:msgType(1) + neogotieteType(1) + neogotiateSate(1) + requestTime(8) + json(N)
                        //(2)dcworker 不解析 json 字段,只把剩余字节 parse 成合法 QJsonObject 后透传
                        //(3)explain/类型协商参数(file/video/audio)由 mainwindow 业务侧按 msgType 自取
                        uint64_t requestTime;
                        std::memcpy(&requestTime,bp,8);bp+=8;
                        QJsonObject callParams;
                        int remainLength=binaryMsg.size()-(bp-binaryMsg.data());
                        if(remainLength>0)
                        {
                            QJsonDocument doc=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(bp),remainLength));
                            if(doc.isObject())
                                callParams=doc.object();
                        }
                        emit transferRequest(msgType,requestTime,callParams,this);
                    }
                    else if(neogotiateSate==20)
                    {
                        //按 msgType 区分音视频挂断,避免一个会话挂断后污染另一会话状态
                        if(msgType==TYPE_AUDIO)
                        {
                            isAudioCalling=false;
                            emit audioHangupReceived(peerHostNum);
                        }
                        else
                        {
                            isVideoCalling=false;
                            emit videoHangupReceived(peerHostNum);
                        }
                    }
                    else
                    {
                        //0/1 响应路径:wire 格式保持 msgType(1) + neogotieteType(1) + requestTime(8)
                        //(createResponse 内写入 8 字节 requestTime,sender 用此关联到 mission lambda)
                        uint64_t requestTime;
                        std::memcpy(&requestTime,bp,8);bp+=8;
                        emit returnRequestResult(requestTime,neogotiateSate);
                    }
                    break;
                }
                case TYPE_TUN://TUN隧道数据
                {
                    QMutexLocker locker(inboundBufferMutex);
                    inboundBuffer.emplace_back(binaryMsg.begin()+1,binaryMsg.end());
                    break;
                }
                case TYPE_FILE://文件分块传输
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

                    qLog()<<"[FILE-RECV] 收到文件块 fileName="<<fileName<<" chunkIndex="<<chunkIndex<<"/"<<chunkAmount
                         <<" dataSize="<<binaryMsg.size()-(bp-binaryMsg.data())<<" workerIndex="<<index;

                    if(!fileContainer.contains(fileName))
                    {
                        qLog()<<"[FILE-RECV] 首块,调用startSingleFileDownLoader";
                        QMetaObject::invokeMethod(this,"startSingleFileDownLoader",Qt::BlockingQueuedConnection,Q_ARG(const QString&,fileName),Q_ARG(uint64_t,chunkAmount));
                        qLog()<<"[FILE-RECV] startSingleFileDownLoader返回, fileContainer.contains="<<fileContainer.contains(fileName);
                        state.fileState.emplace_back(fileName,peerHostNum);
                    }
                    filedownloader* dl=fileContainer.value(fileName).first;
                    qLog()<<"[FILE-RECV] 准备invokeMethod writeToChunkIndex, dl="<<dl<<" dl->running="<<dl->running;
                    QMetaObject::invokeMethod(dl,"writeToChunkIndex",Qt::QueuedConnection,
                                              Q_ARG(uint64_t,chunkIndex),
                                              Q_ARG(const QByteArray&,QByteArray(
                                                                            (const char*)bp,binaryMsg.size()-(bp-binaryMsg.data())
                                                                            ))) ;
                    free(filename);
                    break;
                }
                case TYPE_VIDEO://视频帧
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
                case TYPE_AUDIO://音频帧
                {
                    if(!isShuttingDown && isAudioCalling)
                    {
                        //wire format: TYPE_AUDIO(1B) + sr_code(2B) + ch(1B) + opus_payload(NB)
                        if(binaryMsg.size() < 1 + 3 + 1)
                            break;//帧太短,丢弃

                        uint16_t srCode;
                        std::memcpy(&srCode, bp, 2); bp+=2;
                        uint8_t ch;
                        std::memcpy(&ch, bp, 1); bp+=1;
                        int remoteSr = srCode * 1000;
                        int remoteCh = ch;

                        int payloadBytes = binaryMsg.size() - (bp - binaryMsg.data());

                        //首帧:创建 decoder;后续帧:格式变化时重建
                        if(!audioDeCoder || audioCallSampleRate != remoteSr || audioCallChannelCount != remoteCh)
                        {
                            if(audioDeCoder)
                            {
                                QMetaObject::invokeMethod(audioDeCoder,"shutdown",Qt::BlockingQueuedConnection);
                                audioDeCoder->deleteLater();
                                audioDeCoderTrd->quit();
                                audioDeCoderTrd->wait(1000);
                                audioDeCoderTrd->deleteLater();
                                audioDeCoder = nullptr;
                                audioDeCoderTrd = nullptr;
                            }
                            QMetaObject::invokeMethod(this,"startAudioDeCoder",Qt::BlockingQueuedConnection,
                                                      Q_ARG(int,remoteSr),Q_ARG(int,remoteCh));
                        }

                        std::byte* singleFrameData=(std::byte*)malloc(payloadBytes);
                        std::memcpy(singleFrameData,bp,payloadBytes);
                        QMetaObject::invokeMethod(audioDeCoder,"decodedFlood",Qt::QueuedConnection,
                                                  Q_ARG(void*,(void*)singleFrameData),Q_ARG(int,payloadBytes));
                    }
                    break;
                }
                case TYPE_JSON:
                {
                    QJsonDocument doc=QJsonDocument::fromJson(QByteArray((char*)(bp),binaryMsg.size()-1));
                    if(doc.isObject())
                        emit signalingBackUp(doc.object());
                    //需要独立信号
                    //dcworker::sendSignalingMsg=>dcmanager=>jsonWorker::onInternalMsg
                    //dcworker::signalingBackUp=>dcmanager=>jsonWorker::onExternalMsg=>复用networker原有逻辑
                    //仅在离线模式下启用signaling在dc上的扩展逻辑 保持onlineMode下的signaling统一走tcp的逻辑
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

public slots://startAudioDeCoderSlot
    //由首帧 audioMsg header 中的格式参数构造 decoder
    void startAudioDeCoder(int sr, int ch)
    {
        audioCallSampleRate = sr;
        audioCallChannelCount = ch;
        (audioDeCoder=new audiodecoder(sr,ch))->moveToThread(audioDeCoderTrd=new QThread);
        connect(audioDeCoder,&audiodecoder::sendDecodedAudio,this,[this](const QByteArray& pcmData){
            emit transferDecodedAudio(pcmData,peerHostNum);
        });
        connect(audioDeCoder, &audiodecoder::sendDecodedAudioLevel, this,
                [this](int level){
            emit transferDecodedAudioLevel(level, peerHostNum);
        });
        audioDeCoderTrd->start();
    }

public slots://startFileDownLoadSlot
    void startSingleFileDownLoader(const QString& fileName,uint64_t chunkAmount)
    {
        qLog()<<"[FILE-RECV] startSingleFileDownLoader调用 fileName="<<fileName<<" chunkAmount="<<chunkAmount<<" 当前线程="<<QThread::currentThread();
        filedownloader* worker;QThread* trd;
        fileContainer.insert(fileName,QPair<filedownloader*,QThread*>(worker=new filedownloader(fileName,chunkAmount),trd=new QThread));
        connect(worker,&filedownloader::fileWriteFinished,this,[this,trd](const QString& fileName){
            emit informFileDownLoadFinish(fileName,peerHostNum);
            fileContainer.remove(fileName);
            trd->quit();
        });
        connect(trd,&QThread::finished,this,[worker,trd](){
            delete(worker);
            trd->deleteLater();
        });
        connect(worker,&filedownloader::fileWriteProgress,this,[this](const QString& fileName,qreal progress){
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
        qLog()<<"[FILE-RECV] downloader已启动 fileName="<<fileName<<" dlThread="<<trd<<" running="<<worker->running;
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
                {
                    isBufferBusy=true;
                    //仅当忙时开启自动消费pendingMsg
                    if(!processPendingTimer->isActive())
                        processPendingTimer->start();
                }
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
            else if(index==1)
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
                {
                    isBufferBusy=true;
                    if(!processPendingTimer->isActive())
                        processPendingTimer->start();
                }
            } catch (const std::exception& e)
            {
                qWarning() << "dc binary send failed:" << e.what();
                dcValid = false;
            }
        }
    }

public://pendingDataFetcher
    bool getBinaryMsg()
    {//固定优先级: audio > video > tun > file
        if(pendingAudio&&!(pendingAudio->isEmpty()))
        {
            *tempByteArrayPtr=pendingAudio->dequeue();
            return true;
        }
        if(pendingFrame&&!(pendingFrame->isEmpty()))
        {
            *tempByteArrayPtr=pendingFrame->dequeue();
            return true;
        }
        if(pendingTun&&!(pendingTun->isEmpty()))
        {
            *tempByteArrayPtr=pendingTun->top();
            pendingTun->pop();
            return true;
        }
        if(pendingFile&&!(pendingFile->isEmpty()))
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
        bool msgIsValid=false;
        do
        {
            if(!isBufferBusy)
                if(dc&&dc->isOpen()&&dcValid)
                {
                    if(pendingString&&!pendingString->isEmpty())
                        sendMsg(pendingString->dequeue());
                    else
                    {
                        if(msgIsValid=getBinaryMsg())
                        {
                            sendMsg(*tempByteArrayPtr);
                            if(!processPendingTimer->isActive())
                                processPendingTimer->start();
                        }
                        else
                            if(processPendingTimer->isActive())
                                processPendingTimer->stop();//避免不必要空转
                        //分析一下问题
                        //remainBinaryMsg=getBinaryMsg() + while(...&&remainBinaryMsg)
                        //预期应该是在获取一次binaryMsg后判断下次是否可能还有信息要发(是否pendingContanier为空)
                        //但这里有两方面的问题
                        //(1)getBinary的返回值没有被立刻使用=>这意味着此时tempByteArrayPtr*这个指针指向的byteArray内存并没有被更新
                        //结果就是发送的 要么是无效数据(初始化时)(几乎不可能发生) 要么就是不久前发送过一次的'旧'数据 ★
                        //(2)processPenddingMsg这个函数的调用源有两个大类
                        //1.每次发送binaryMsg后通过predictNext预测接下来短时间内是否还有数据包,若无则可利用短暂间隙消费pendingMsg
                        //2.每100ms发射一次timeour信号的processPendingTimer 目的是即便上游(MainWindow)总是投递binaryMsg到worker也总能至少对pendingMsg消费一次 避免积压数据'饿死' ★
                        //而在本例(现在是2026/6/7/16:12/)中观察到的'Peer切块完成后',"Peer的出站网速和Coor的入站网速"始终稳定在200KB/S
                        //而且还能观察到文件接收端(Coor)的日志结尾出现大量连续的'尾块'重复输出(do中读取旧byteArray+timer高频触发) 这就是上述两个★所致
                        //修改建议
                        //(1)processPendingTimer的信号发射频率考虑在运行时动态修改
                        //(2)每次getBinaryMsg返回后其返回值必须立刻看作msgIsValid判断(不再作为remainBinaryMsg仅用于判断是否还残余pendingMsg待处理)
                        state.pressure=(pendingTun?pendingTun->size():0)+(pendingFile?pendingFile->size():0)
                        +(pendingAudio?pendingAudio->size():0)+(pendingFrame?pendingFrame->size():0);
                    }
                }
        }
        while(!newEventNow&&!isBufferBusy&&msgIsValid);
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
                emit dcConnected(peerHostNum);
                QMetaObject::invokeMethod(this,"vade",Qt::QueuedConnection);
                dcValid=true;
                dc->setBufferedAmountLowThreshold(freeSize);
                dc->onBufferedAmountLow([this]() {
                    isBufferBusy=false;
                });
                dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                    onDcMsg(message);
                });
                //初始不开启 仅当繁忙时开启 避免不必要空转
                // QMetaObject::invokeMethod(this,"startProcessPendingStackTimer",Qt::QueuedConnection);
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
                        //processer->stop()与此非强关联 降到freeSize和剩余pending量有关但没有必然关系
                        //(但是降到freeSize大概率是没有pendingMsg了倒是真的,因为缓冲区总量下降证明上游生产速度小于下游消费速度
                        //而从理论上来讲下游消费速度不可能大于上游生产速度 因为上游是直接读取连续内存内已经存放的msg而下游和网络链路质量直接相关且本身也存在一定上限)
                        //但总的来讲从语义上来看 stop还是放在判断pendingContainer为空的位置比较好
                    });
                    dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                        onDcMsg(message);
                    });
                    // QMetaObject::invokeMethod(this,"startProcessPendingStackTimer",Qt::QueuedConnection);
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
                remoteDescSet=true;
                // 刷新缓冲的 candidate
                for(const auto& c : pendingCandidates)
                    pc->addRemoteCandidate(rtc::Candidate(c.first.toStdString(),c.second.toStdString()));
                pendingCandidates.clear();
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
            //远端offerER传递offer/candidate=>本地dcManager收到offer/candidate=>addPeer创建answerER并返回dcworker*
            //=>异步投递setRemoteSdp事件

            if(remoteDescSet)
                pc->addRemoteCandidate(rtc::Candidate(sdp.toStdString(), mediaType.toStdString()));
            else
                pendingCandidates.append(qMakePair(sdp,mediaType));
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
        if(audioDeCoder)
        {
            audioDeCoder->deleteLater();
            audioDeCoderTrd->quit();
            audioDeCoderTrd->wait();
            audioDeCoderTrd->deleteLater();
            audioDeCoder=nullptr;
            audioDeCoderTrd=nullptr;
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
    void receiveStringMsg(int peerHostNum, const QString& msg);
    void informFileDownLoadFinish(const QString& filename,int peerHostNum);
    void transferDecodedFrame(const QImage&,int);
    void transferDecodedAudio(const QByteArray& pcmData,int peerHostNum);
    //(新增)对端每帧解码后的相对音量 0~100 -> 由 dcmanager 透传给 mainwindow 弹窗
    void transferDecodedAudioLevel(int level,int peerHostNum);
    void transferRequest(uint8_t msgType,uint64_t requestTime,const QJsonObject& callParams,void* voidDCWorker);
    void returnRequestResult(uint64_t requestTime,bool result);
    void videoHangupReceived(int peerHostNum);
    void audioHangupReceived(int peerHostNum);
    void dcConnected(int peerHostNum);

    void sendSignalingMsg(const QJsonObject&);
    void signalingBackUp(const QJsonObject&);
};
#endif
