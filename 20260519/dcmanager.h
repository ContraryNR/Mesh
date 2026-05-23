#ifndef DCMANAGER_H
#define DCMANAGER_H

#include <QJsonArray>
#include "dcworker.h"

#define isOffer true
#define isntOffer false

class dcmanager : public QObject
{
    Q_OBJECT
public:
    QHash<int,QString> nameRoute;
    QHash<int,dcworker*> ipRoute;
    std::vector<rtc::binary>& inboundBuffer;
    std::atomic<int> inboundSpeed=0;
    std::atomic<int> outboundSpeed=0;
    QTimer* sendSpeedTimer{NULL};
    QMutex* mutex{NULL};
    dcmanager(std::vector<rtc::binary>& inBuffer,QMutex* mtx):inboundBuffer(inBuffer),mutex(mtx)
    {
        sendSpeedTimer=new QTimer(this);
        sendSpeedTimer->setInterval(1000);
        connect(sendSpeedTimer,&QTimer::timeout,this,[this](){
            emit sendInboundSpeed(inboundSpeed);
            inboundSpeed=0;
            emit sendOutboundSpeed(outboundSpeed);
            outboundSpeed=0;
        });
    }
public://basicFunc
    dcworker* addPeer(const QString& peerHostName,int peerHostNum,bool isOfferER)
    {
        nameRoute.insert(peerHostNum,peerHostName);
        dcworker* worker=new dcworker(isOfferER,peerHostNum,inboundBuffer,mutex);
        ipRoute.insert(peerHostNum,worker);

        connect(worker,&dcworker::transferMsg,this,&dcmanager::transferMsg);
        connect(worker, &dcworker::sendInboundSpeed, this, &dcmanager::addInboundSpeed);
        connect(worker, &dcworker::sendOutboundSpeed, this, &dcmanager::addOutboundSpeed);
        connect(worker, &dcworker::receiveStringMsg, this, &dcmanager::receivePeerMsg);

        QThread* trd=new QThread;
        worker->moveToThread(trd);

        connect(worker,&dcworker::dcFinish,this,[worker,trd,peerHostNum,this](){
            trd->quit();
            trd->wait();
            delete(worker);
            trd->deleteLater();
            QString peerName = nameRoute.value(peerHostNum);
            ipRoute.remove(peerHostNum);
            nameRoute.remove(peerHostNum);
            emit peerRemoved(peerHostNum, peerName);
        });

        trd->start();

        QMetaObject::invokeMethod(worker,"createDc",Qt::QueuedConnection);
        emit peerAdded(peerHostNum, peerHostName);
        return worker;
    }
public slots://workerFunc
    void createOfferER(const QJsonArray& hostNameList,const QJsonArray& hostNumList)
    {
        int peerHostNum;
        for(int i=0;i<hostNumList.size();i++)
            if(!nameRoute.contains(peerHostNum=hostNumList[i].toInt()))
                addPeer(hostNameList[i].toString(),peerHostNum,isOffer);
    }
    void createAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer)
    {
        if(!nameRoute.contains(peerHostNum))
        {
            dcworker* worker=addPeer(peerHostName,peerHostNum,isntOffer);
            QMetaObject::invokeMethod(worker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,offer),Q_ARG(const QString&,"offer"));
        }
    }
    void setCandidate(const QString& candidate,const QString& mid,int peerHostNum)
    {
        dcworker* worker=ipRoute.value(peerHostNum,nullptr);
        if(worker)
            QMetaObject::invokeMethod(worker,"receiveCandidate",Qt::QueuedConnection,Q_ARG(const QString&,candidate),Q_ARG(const QString&,mid));
    }
    void setAnswer(const QString& sdp,int peerHostNum)
    {
        dcworker* worker=ipRoute.value(peerHostNum,nullptr);
        if(worker)
            QMetaObject::invokeMethod(worker,"setRemoteSdp",Qt::QueuedConnection,Q_ARG(const QString&,sdp),Q_ARG(const QString&,"answer"));
    }

public slots://(向内)链路聚合函数
    void transferMsg(const QJsonObject& msg)
    {emit sendMsg(msg);}
    void addInboundSpeed(int ins)
    {inboundSpeed+=ins;}
    void addOutboundSpeed(int outs)
    {outboundSpeed+=outs;}
    void receivePeerMsg(int peerHostNum, const QString& msg)
    {emit receiveStringMsg(peerHostNum, nameRoute.value(peerHostNum, QString("未知")), msg);}

public slots://(向外)链路拆分函数
    void sendStringToPeer(int peerHostNum, const QString& msg)
    {
        dcworker* worker = ipRoute.value(peerHostNum, nullptr);
        if (worker)
        {
            worker->newEventNow=true;
            QMetaObject::invokeMethod(worker, "sendStringMsg", Qt::QueuedConnection,
                                      Q_ARG(const QString&, msg));
        }
    }
    void sendByteArrayToPeer(int peerHostNum,const QByteArray& msg,bool predictNextEvent)
    {
        dcworker* worker = ipRoute.value(peerHostNum, nullptr);
        if (worker)
        {
            worker->newEventNow=true;
            QMetaObject::invokeMethod(worker, "sendBinaryMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, msg),Q_ARG(bool,predictNextEvent));
        }
    }
    void broadcastString(const QString& msg)
    {
        for (auto it = ipRoute.begin(); it != ipRoute.end(); ++it)
        {
            it.value()->newEventNow=true;
            QMetaObject::invokeMethod(it.value(), "sendStringMsg", Qt::QueuedConnection,
                                      Q_ARG(const QString&, msg));
        }
    }
public slots://timerControlFunc
    void startTimer()
    {
        sendSpeedTimer->start();
    }
    void stopTimer()
    {
        sendSpeedTimer->stop();
    }
    void cleanQOBJ()
    {
        sendSpeedTimer->stop();
        delete(sendSpeedTimer);
    }
signals:
    void sendMsg(const QJsonObject&);
    void sendInboundSpeed(int);
    void sendOutboundSpeed(int);
    void peerAdded(int peerHostNum, const QString& peerHostName);
    void peerRemoved(int peerHostNum, const QString& peerHostName);
    void receiveStringMsg(int peerHostNum, const QString& peerName, const QString& msg);
};

#endif // DCMANAGER_H
