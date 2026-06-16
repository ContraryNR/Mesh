#ifndef DCMANAGER_H
#define DCMANAGER_H

#include <QJsonArray>
#include <QDebug>
#include "dcworker.h"

#define maxWorkerGroupSize 4
//worker index: 0=>主通道(TUN/字符串/协商) 1=>文件传输 2=>音频(暂未实现) 3=>视频通话
//TYPE_*宏仅用于消息协议头和purpose参数,与workerGroup中的index解耦

class dcmanager : public QObject
{
    Q_OBJECT
public:
    QHash<int,QString> nameRoute;
    QHash<int,QVector<dcworker*>> ipRoute;
    QHash<QPair<int,int>, QVector<QPair<QString,QString>>> candidateBuffer;
    bool onlineMode;
    // candidate 缓冲: worker 不存在时暂存, 创建后自动刷新
    //逻辑上可能缓冲在jsonWorker会好一点 但是放在dcManager确实可以避免回溯
    //(不然得在jsonWorker先判断worker是否为空然后创建时还得等dcManager发通知)
    std::vector<rtc::binary>& inboundBuffer;
    QTimer* basicTimer{nullptr};
    QMutex* mutex{NULL};
    int busySize=104857;
    int freeSize=32768;
    dcmanager(std::vector<rtc::binary>& inBuffer,QMutex* mtx,bool isOnline):inboundBuffer(inBuffer),mutex(mtx),onlineMode(isOnline)
    {
        basicTimer=new QTimer(this);
        basicTimer->setInterval(1000);
        connect(basicTimer,&QTimer::timeout,this,[this](){
            int ibs=0,obs=0,pr=0;
            QList<fileDownLoadState> allState;
            for(auto workerGroup:ipRoute.values())
                for(dcworker* worker:workerGroup)
                    if(worker)
                    {
                        workerState state=worker->getState();
                        ibs+=state.inBoundSpeed;
                        obs+=state.outBoundSpeed;
                        pr+=state.pressure;
                        allState<<state.fileState;
                    }
            emit workerStatePulse(ibs,obs,pr,allState);
        });        
    }

    //peerHostName不用默认参(QString())也行=>上游显式判断是否存在有效peerHostName(非worker0不传hostName)
    dcworker* addPeer(const QString& peerHostName,int peerHostNum,bool isOfferER)
    {
        if(!nameRoute.contains(peerHostNum)&&(!peerHostName.isEmpty()))
        {
            nameRoute.insert(peerHostNum,peerHostName);
            ipRoute.insert(peerHostNum,QVector<dcworker*>());
        }

        int index=ipRoute[peerHostNum].size();
        if(index>=maxWorkerGroupSize)
            return nullptr;

        dcworker* worker=new dcworker(isOfferER,peerHostNum,index,inboundBuffer,mutex,busySize,freeSize);
        ipRoute[peerHostNum].append(worker);

        // emit peerConnectionAmountChanged(peerHostNum,ipRoute[peerHostNum].size());

        connect(worker,&dcworker::sendSignalingMsg,this,[this](const QJsonObject& msg){
            emit transferWorkerMsg(msg);
        });
        connect(worker, &dcworker::receiveStringMsg, this,[this](int peerHostNum, const QString& msg){
            emit receiveStringMsg(peerHostNum, msg);
        });
        connect(worker,&dcworker::informFileDownLoadFinish,this,[this](const QString& fileName,int peerHostNum){
            emit informFileDownLoadFinish(fileName,peerHostNum);
        });
        connect(worker,&dcworker::transferDecodedFrame,this,[this](const QImage& frame,int peerHostNum){
            emit transferDecodedFrame(frame,peerHostNum);
        });
        connect(worker,&dcworker::transferDecodedAudio,this,[this](const QByteArray& pcmData,int peerHostNum){
            emit transferDecodedAudio(pcmData,peerHostNum);
        });
        //(新增)对端音频帧解码后的相对音量:worker 线程 -> dcmanager 线程 -> mainwindow
        connect(worker,&dcworker::transferDecodedAudioLevel,this,[this](int level,int peerHostNum){
            emit transferDecodedAudioLevel(level,peerHostNum);
        });
        connect(worker,&dcworker::transferRequest,this,[this](uint8_t msgType,uint64_t requestTime,const QJsonObject& callParams,void* voidDCWorker){
            emit transferRequest(msgType,requestTime,callParams,voidDCWorker);
        });
        connect(worker,&dcworker::returnRequestResult,this,[this](uint64_t requestTime,bool result){
            emit returnRequestResult(requestTime,result);
        });
        connect(worker,&dcworker::videoHangupReceived,this,[this](int peerHostNum){
            emit videoHangupReceived(peerHostNum);
        });
        connect(worker,&dcworker::audioHangupReceived,this,[this](int peerHostNum){
            emit audioHangupReceived(peerHostNum);
        });

        connect(worker,&dcworker::dcConnected,this,[this](int peerHostNum){
            emit dcConnected(nameRoute.value(peerHostNum,"未知主机"));
        });

        if(!onlineMode)
            connect(worker,&dcworker::signalingBackUp,this,[this](const QJsonObject& signalingMsg){
                emit onSignalingBackMsg(signalingMsg);
            });

        QThread* trd=new QThread;
        worker->moveToThread(trd);
        connect(worker,&dcworker::dcFinish,this,[worker,trd,peerHostNum,this,index](){
            trd->quit();
            trd->wait();
            delete(worker);
            ipRoute[peerHostNum].removeAll(worker);
            trd->deleteLater();
            QString peerName = nameRoute.value(peerHostNum);
            if(index==0)
            {
                ipRoute.remove(peerHostNum);
                nameRoute.remove(peerHostNum);
                emit peerRemoved(peerHostNum);
            }
            else
                emit peerConnectionAmountChanged(peerHostNum,ipRoute[peerHostNum].size());
        });
        trd->start();
        QMetaObject::invokeMethod(worker,"createDc",Qt::QueuedConnection);

        if(index==0)
            emit peerAdded(peerHostNum, peerHostName);
        else
            emit peerConnectionAmountChanged(peerHostNum,ipRoute[peerHostNum].size());
        return worker;
    }

public slots://signalingSlot
    //jsonWorker解析出initialOffer建立初始worker0走createOfferER槽函数
    //settingDialog走getExtraConnection槽函数间接调用addPeer而不是createOfferER
    void createOfferER(const QString& peerHostName,int peerHostNum)//=>只负责初始dc建立
    {
        if(nameRoute.contains(peerHostNum))
            return;
        addPeer(peerHostName,peerHostNum,true);
    }
    void createAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer,int index)
    {
        dcworker* currentWorker;
        //(1)判断是否已加入(基于workerGrpSize) (2)判断字符串是否为空(基于对方index是否为0)
        if(!nameRoute.contains(peerHostNum)&&(!peerHostName.isEmpty()))
        {
            currentWorker=addPeer(peerHostName,peerHostNum,false);
            if(currentWorker)
                QMetaObject::invokeMethod(currentWorker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,offer),Q_ARG(const QString&,"offer"));
        }
        else
        {
            if(index>=maxWorkerGroupSize)return;
            int currentSize=ipRoute[peerHostNum].size();
            for(int i=0;i<index+1-currentSize;i++)
                //根据对方worker的index创建连续的直到同index的worker
                addPeer(peerHostName,peerHostNum,false);
            if(index < ipRoute[peerHostNum].size())
            {
                currentWorker=ipRoute[peerHostNum][index];
                if(currentWorker)
                    QMetaObject::invokeMethod(currentWorker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,offer),Q_ARG(const QString&,"offer"));
            }
        }

        //初始连接offer/连接补齐完成后当前offer=>对应的worker
        //连接补齐的非投递目标的worker与此无关
        //只有answerER可能存在candidate投递到'pcState!=0'或'!worker'的情况
        //不需要放在addPeer统一执行

        //你收到的如果是answer,那你必然已经有了对应的offerER=>此前不会缓冲candidate
        //你收到的如果是offer,这种情况下才不确定是否有对应的answerER=>此前可能缓冲candidate

        // 刷新该 worker 缓冲的 candidate
        QPair<int,int> bufKey(peerHostNum,index);
        if(candidateBuffer.contains(bufKey))
        {
            for(const auto& p : candidateBuffer.take(bufKey))
                QMetaObject::invokeMethod(currentWorker,"receiveCandidate",Qt::QueuedConnection,Q_ARG(const QString&,p.first),Q_ARG(const QString&,p.second));
        }
    }
    void setAnswer(const QString& sdp,int peerHostNum,int index)
    {
        if(index>=ipRoute[peerHostNum].size())return;
        dcworker* worker=ipRoute.value(peerHostNum)[index];
        if(worker)
            QMetaObject::invokeMethod(worker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,sdp),Q_ARG(const QString&,"answer"));
    }
    void setCandidate(const QString& candidate,const QString& mid,int peerHostNum,int index)
    {
        if(!ipRoute.contains(peerHostNum)||index>=ipRoute[peerHostNum].size()||!ipRoute[peerHostNum][index])
        {
            // worker尚未创建,缓冲 candidate 避免candidate无效投递
            candidateBuffer[QPair<int,int>(peerHostNum,index)].append(qMakePair(candidate,mid));
            return;
        }
        dcworker* worker=ipRoute[peerHostNum][index];
        QMetaObject::invokeMethod(worker,"receiveCandidate",Qt::QueuedConnection,Q_ARG(const QString&,candidate),Q_ARG(const QString&,mid));
    }

public slots://connectionExtensionSlot
    void getExtraConnection(int targetAmount,int peerHostNum)
    {
        int currentSize=ipRoute[peerHostNum].size();
        for(int i=0;i<targetAmount-currentSize;i++)
        {
            addPeer(QString(),peerHostNum,true);
        }
    }
    void releaseExtraConnection(int targetAmount,int peerHostNum)
    {
        int offset=ipRoute[peerHostNum].size()-targetAmount;
        for(int i=0;i<offset;i++)
        {
            QMetaObject::invokeMethod(ipRoute[peerHostNum].back(),"shutdown",Qt::QueuedConnection);
            ipRoute[peerHostNum].pop_back();
        }
    }

public slots://outBoundBufferSettingSlot
    void updateAllDcWorkerSettings(int bSize, int fSize)
    {
        busySize = bSize;
        freeSize = fSize;
        for (QVector<dcworker*>& workerGroup : ipRoute.values())
            for(dcworker* worker:workerGroup)
            QMetaObject::invokeMethod(worker, "updateSettings", Qt::QueuedConnection,Q_ARG(int, bSize), Q_ARG(int, fSize));
    }

public slots://timerSlot
    void startTimer()
    {
        basicTimer->start();
    }
    void stopTimer()
    {
        if(basicTimer->isActive())
            basicTimer->stop();
    }
    void cleanQOBJ()
    {
        if(basicTimer->isActive())
            basicTimer->stop();
        basicTimer->deleteLater();
    }

signals:
    void transferWorkerMsg(const QJsonObject&);
    void pendingBinaryMsgSizeChanged(int pendingSize);
    void peerAdded(int peerHostNum, const QString& peerHostName);
    void peerRemoved(int peerHostNum);
    void receiveStringMsg(int peerHostNum, const QString& msg);
    void informFileDownLoadFinish(const QString& filename,int peerHostNum);
    void transferDecodedFrame(const QImage&,int);
    void transferDecodedAudio(const QByteArray&,int);
    //(新增)对端音频帧解码后的相对音量透传给 mainwindow
    void transferDecodedAudioLevel(int level,int peerHostNum);
    void peerConnectionAmountChanged(int peerHostNum,int currentConnectAmount);
    void workerStatePulse(int ibs,int obs,int pr,const QList<fileDownLoadState>&);
    void transferRequest(uint8_t msgType,uint64_t requestTime,const QJsonObject& callParams,void* voidDCWorker);
    void returnRequestResult(uint64_t requestTime,bool result);
    void videoHangupReceived(int peerHostNum);
    void audioHangupReceived(int peerHostNum);
    void dcConnected(const QString& hostName);
    void onSignalingBackMsg(const QJsonObject&);
};

inline dcworker* getDcWorker(void* voidIpRoute,int peerHostNum,int purpose)
{
    if(!voidIpRoute)return nullptr;
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty()||!ipRoute->contains(peerHostNum))return nullptr;
    QVector<dcworker*> workerGroup=ipRoute->value(peerHostNum);
    dcworker* worker=nullptr;
    int idx=-1;
    switch(purpose)
    {
        case TYPE_TUN:  idx=0;break;
            //MIMO2.5P你tm神了 tmd完全不考虑未增大对外连接数的情况
        // case TYPE_FILE: idx=;break;
        case TYPE_FILE: idx=workerGroup.size()>1?1:0;break;
        case TYPE_AUDIO:idx=2;break;
        case TYPE_VIDEO:idx=3;break;
    }
    if(idx>=0&&idx<workerGroup.size())
    {
        worker=workerGroup[idx];
        if(idx==0)worker->newEventNow=true;
    }
    return worker;
}
inline QList<dcworker*> getBroundCastWorkers(void* voidIpRoute)
{
    if(!voidIpRoute)return QList<dcworker*>();
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty())return QList<dcworker*>();
    QList<dcworker*> workers;
    for(auto workerGroup:ipRoute->values())
        workers.append(workerGroup[0]);
    return workers;
}

inline QList<dcworker*> getCallingPeerWorkers(void* voidIpRoute,QList<int> peerHostNumList)
{
    if(!voidIpRoute)return QList<dcworker*>();
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty())return QList<dcworker*>();
    QList<dcworker*> workers;
    QVector<dcworker*> tempWorkerGrp;
    for(int hostNum:peerHostNumList)
        if(ipRoute->contains(hostNum))
        {
            tempWorkerGrp=ipRoute->value(hostNum);
            if(tempWorkerGrp.size()>=4)
                workers.append(tempWorkerGrp[3]);
        }
    return workers;
}

//音频对应的 worker[2] 收集器(镜像 getCallingPeerWorkers,但索引为音频槽)
inline QList<dcworker*> getCallingPeerAudioWorkers(void* voidIpRoute,QList<int> peerHostNumList)
{
    if(!voidIpRoute)return QList<dcworker*>();
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty())return QList<dcworker*>();
    QList<dcworker*> workers;
    QVector<dcworker*> tempWorkerGrp;
    for(int hostNum:peerHostNumList)
        if(ipRoute->contains(hostNum))
        {
            tempWorkerGrp=ipRoute->value(hostNum);
            if(tempWorkerGrp.size()>=3)
                workers.append(tempWorkerGrp[2]);
        }
    return workers;
}

inline bool isWorkerReady(int peerHostNum,int purpose,void* voidIpRoute)
{
    if(!voidIpRoute)return false;
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty())return false;
    int idx=-1;
    switch(purpose)
    {
        case TYPE_TUN:  idx=0;break;
        case TYPE_FILE: idx=1;break;
        case TYPE_AUDIO:idx=2;break;
        case TYPE_VIDEO:idx=3;break;
    }
    if(idx>=0&&ipRoute->contains(peerHostNum)&&ipRoute->value(peerHostNum).size()>idx)
        return true;
    return false;
}

//注意 独立inline函数定义在#endif上面而不是下面
#endif // DCMANAGER_H
