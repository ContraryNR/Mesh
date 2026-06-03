#ifndef DCMANAGER_H
#define DCMANAGER_H

#include <QJsonArray>
#include <QDebug>
#include "dcworker.h"

#define isOffer true
#define isntOffer false
#define maxSize 4
//0=>tun/String
//1=>file
//2=>audio
//3=>video
#define maxIndex 3

class dcmanager : public QObject
{
    Q_OBJECT
public:
    QHash<int,QString> nameRoute;
    QHash<int,QVector<dcworker*>> ipRoute;
    std::vector<rtc::binary>& inboundBuffer;
    QTimer* basicTimer{nullptr};
    QMutex* mutex{NULL};
    int busySize=104857;
    int freeSize=32768;
    dcmanager(std::vector<rtc::binary>& inBuffer,QMutex* mtx):inboundBuffer(inBuffer),mutex(mtx)
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
            workerStatePulse(ibs,obs,pr,allState);
        });        
    }

public:
    dcworker* addPeer(const QString& peerHostName,int peerHostNum,bool isOfferER)
    {
        if(!nameRoute.contains(peerHostNum)&&(!peerHostName.isEmpty()))
        {
            nameRoute.insert(peerHostNum,peerHostName);
            ipRoute.insert(peerHostNum,QVector<dcworker*>());
        }

        int index=ipRoute[peerHostNum].size();
        if(index>=maxSize)
            return nullptr;

        dcworker* worker=new dcworker(isOfferER,peerHostNum,index,inboundBuffer,mutex,busySize,freeSize);
        ipRoute[peerHostNum].append(worker);

        emit peerConnectionAmountChanged(peerHostNum,ipRoute[peerHostNum].size());

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
        connect(worker,&dcworker::transferRequest,this,[this](int requestFlag,uint64_t requestTime,const QString& explain,void* voidDCWorker){
            emit transferRequest(requestFlag,requestTime,explain,voidDCWorker);
        });
        connect(worker,&dcworker::returnRequestResult,this,[this](uint64_t requestTime,bool result){
            emit returnRequestResult(requestTime,result);
        });
        connect(worker,&dcworker::videoHangupReceived,this,[this](int peerHostNum){
            emit videoHangupReceived(peerHostNum);
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

public slots://newWorkerSlot
    void createOfferER(const QJsonArray& hostNameList,const QJsonArray& hostNumList)
    {
        int peerHostNum;
        for(int i=0;i<hostNumList.size();i++)
            if(!nameRoute.contains(peerHostNum=hostNumList[i].toInt()))
                addPeer(hostNameList[i].toString(),peerHostNum,isOffer);
    }
    void createAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer,int index)
    {
        if(!nameRoute.contains(peerHostNum))
        {
            dcworker* worker=addPeer(peerHostName,peerHostNum,isntOffer);
            QMetaObject::invokeMethod(worker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,offer),Q_ARG(const QString&,"offer"));
        }
        else
        {
            if(index>maxIndex)return;
            int currentSize=ipRoute[peerHostNum].size();
            for(int i=0;i<index+1-currentSize;i++)
                addPeer(peerHostName,peerHostNum,isntOffer);
            QMetaObject::invokeMethod(ipRoute[peerHostNum][index],"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,offer),Q_ARG(const QString&,"offer"));
        }
    }

public slots://connectionExtensionSlot
    void getExtraConnection(int targetAmount,int peerHostNum)
    {
        int currentSize=ipRoute[peerHostNum].size();
        for(int i=0;i<targetAmount-currentSize;i++)
        {
            addPeer(QString(),peerHostNum,isOffer);
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

public slots://signalingSlot
    void setCandidate(const QString& candidate,const QString& mid,int peerHostNum,int index)
    {
        if(index>=ipRoute[peerHostNum].size())return;
        dcworker* worker=ipRoute.value(peerHostNum)[index];
        if(worker)
            QMetaObject::invokeMethod(worker,"receiveCandidate",Qt::QueuedConnection,Q_ARG(const QString&,candidate),Q_ARG(const QString&,mid));
    }
    void setAnswer(const QString& sdp,int peerHostNum,int index)
    {
        if(index>=ipRoute[peerHostNum].size())return;
        dcworker* worker=ipRoute.value(peerHostNum)[index];
        if(worker)
            QMetaObject::invokeMethod(worker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,sdp),Q_ARG(const QString&,"answer"));
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
    void peerConnectionAmountChanged(int peerHostNum,int currentConnectAmount);
    void workerStatePulse(int ibs,int obs,int pr,const QList<fileDownLoadState>&);
    void transferRequest(uint8_t requestFlag,uint64_t requestTime,const QString& explain,void* voidDCWorker);
    void returnRequestResult(uint64_t requestTime,bool result);
    void videoHangupReceived(int peerHostNum);
};

inline dcworker* getDcWorker(void* voidIpRoute,int peerHostNum,int purpose)
{
    if(!voidIpRoute)return nullptr;
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty()||!ipRoute->contains(peerHostNum))return nullptr;
    QVector<dcworker*> workerGroup=ipRoute->value(peerHostNum);
    dcworker* worker=nullptr;
    switch(purpose)
    {
        case 0://Tun/String
        {
            if(workerGroup.size()>=1)
            {
                worker=workerGroup[0];
                worker->newEventNow=true;
            }
            break;
        }
        case 1://file
        {
            if(workerGroup.size()>=2)
                worker=workerGroup[1];
            else if(workerGroup.size()>=1)
            {
                worker=workerGroup[0];
                worker->newEventNow=true;
            }
            break;
        }
        case 2://audio
        {
            if(workerGroup.size()>=3)
                worker=workerGroup[2];
            break;
        }
        case 3://video
        {
            if(workerGroup.size()>=4)
                worker=workerGroup[3];
            break;
        }
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

inline bool isWorkerReady(int peerHostNum,int purpose,void* voidIpRoute)
{
    if(!voidIpRoute)return false;
    QHash<int,QVector<dcworker*>>* ipRoute=(QHash<int,QVector<dcworker*>>*)voidIpRoute;
    if(ipRoute->isEmpty())return false;
    switch(purpose)
    {
        case 0://tun/string
        {
            if(ipRoute->contains(peerHostNum)&&ipRoute->value(peerHostNum).size()>=1)
                return true;
        }
        case 1://file
        {
            if(ipRoute->contains(peerHostNum)&&ipRoute->value(peerHostNum).size()>=1)
                return true;
        }
        case 2://audio
        {
            if(ipRoute->contains(peerHostNum)&&ipRoute->value(peerHostNum).size()>=3)
                return true;
        }
        case 3://video
        {
            if(ipRoute->contains(peerHostNum)&&ipRoute->value(peerHostNum).size()>=4)
                return true;
        }
    }
    return false;
}

//注意 独立inline函数定义在#endif上面而不是下面
#endif // DCMANAGER_H
