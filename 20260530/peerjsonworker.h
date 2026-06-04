#ifndef PEERJSONWORKER_H
#define PEERJSONWORKER_H

#include "basejsonworker.h"
#include <QJsonArray>
#include <QDebug>
#include "logger.h"

class peerjsonworker : public basejsonworker
{
    Q_OBJECT
public:
    QHash<int,QString>& nameRoute;
    peerjsonworker(QString& localhostname,int& localhostnum,QHash<int,QString>& nameRouteFromDcManager,bool onlineRunning=true)
        :basejsonworker(localhostname,localhostnum,onlineRunning),nameRoute(nameRouteFromDcManager){}

public slots:
    void onInternalMsg(const QJsonObject& msg)
    {
        QByteArray byteArr=QJsonDocument(getFinalJson(msg)).toJson(QJsonDocument::Compact);
        if(onlineMode)
        {
            emit sendToNetWorker(byteArr+'\n');
        }
        else
        {
            QString targetName=(msg["type"].toString()!="hostname")?nameRoute[msg["target"].toInt()]:QString("Coordinator");
            saveOrientedFile(targetName,byteArr);
        }
    }
    void onReadySendHostName()
    {
        QJsonObject hostNameJson;
        hostNameJson["type"]="hostname";
        hostNameJson["hostname"]=localHostName;
        QByteArray byteArr=QJsonDocument(hostNameJson).toJson(QJsonDocument::Compact);
        if(onlineMode)
            emit sendToNetWorker(byteArr+'\n');
        else
            saveOrientedFile("Coordinator",byteArr);
    }
    void onExternalMsg(const QJsonObject& msg)
    {
        QString type = msg["type"].toString();
        if(type=="createOffer")
        {
            localHostNum=msg["yourHostNum"].toInt();
            emit hostNumAssigned(localHostNum);
            QJsonArray hostNameList=msg["hostNameList"].toArray(),hostNumList=msg["hostNumList"].toArray();
            if(hostNameList.size()==hostNumList.size())
                emit goCreateOfferER(hostNameList,hostNumList);
        }else if(type=="sdp")
        {
            if(msg["sdpType"].toString()=="offer")
                emit goCreateAnswerER(msg["hostname"].toString(),msg["source"].toInt(),msg["sdp"].toString(),msg["index"].toInt());
            else if(msg["sdpType"].toString()=="answer")
                emit goSetAnswer(msg["sdp"].toString(),msg["source"].toInt(),msg["index"].toInt());//=>toDcManager
        }else if(type=="candidate")
            goSetCandidate(msg["candidateItem"].toString(),msg["candidateMid"].toString(),msg["source"].toInt(),msg["index"].toInt());
    }
signals:
    void sendToNetWorker(const QByteArray&);
    void goCreateOfferER(const QJsonArray&,const QJsonArray&);
    void goCreateAnswerER(const QString&,int,const QString&,int);
    void goSetAnswer(const QString& sdp,int peerHostNum,int);
    void goSetCandidate(const QString&,const QString&,int,int);
    void hostNumAssigned(int);
};

#endif // PEERJSONWORKER_H
