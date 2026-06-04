#ifndef COORJSONWORKER_H
#define COORJSONWORKER_H

#include "basejsonworker.h"
#include <QTcpSocket>
#include <QJsonArray>
#include <QDebug>
#include "logger.h"

class coorjsonworker : public basejsonworker
{
    Q_OBJECT
public:
    QVector<QString> hostNameVec;
    QVector<QTcpSocket*>* hostSocketVec;
    coorjsonworker(QString& localhostname,int& localhostnum,QVector<QTcpSocket*>* hostsocketvec,bool onlineRunning=true)
        :basejsonworker(localhostname,localhostnum,onlineRunning)
    {hostSocketVec=hostsocketvec;}

public slots:
    void onInternalMsg(const QJsonObject& msg)
    {
        QByteArray byteArr=QJsonDocument(getFinalJson(msg)).toJson(QJsonDocument::Compact);
        if(onlineMode)
            emit sendToNetWorker(byteArr+'\n',msg["target"].toInt());
        else
            saveOrientedFile(hostNameVec[msg["target"].toInt()],byteArr);
    }
    void onExternalMsg(const QJsonObject& msg,QTcpSocket* socket)
    {
        QString type = msg["type"].toString();
        if(type=="hostname")
        {
            QString hostName=msg["hostname"].toString();
            bool ifExist=false;int distributedHostNum;
            QJsonArray pendingHostName,pendingHostNum;
            pendingHostName.append(localHostName);
            pendingHostNum.append(1);
            for(auto beg=hostNameVec.begin();beg<hostNameVec.end();beg++)
                if(*beg!=hostName)
                {
                    ifExist=false;
                    if(!((*hostSocketVec)[beg-hostNameVec.begin()]))
                        continue;
                    pendingHostName.append(*beg);
                    pendingHostNum.append(beg-hostNameVec.begin()+2);
                }
            else
            {
                ifExist=true;
                (*hostSocketVec)[beg-hostNameVec.begin()]=socket;
                distributedHostNum=beg-hostNameVec.begin()+2;
            }
            if(!ifExist)
            {
                hostNameVec.append(hostName);
                hostSocketVec->append(socket);
                socket=nullptr;
                distributedHostNum=hostNameVec.size()-1+2;
            }
            QJsonObject createOffer;
            createOffer["type"]="createOffer";
            createOffer["yourHostNum"]=distributedHostNum;
            createOffer["hostNameList"]=pendingHostName;
            createOffer["hostNumList"]=pendingHostNum;
            QByteArray byteArr=QJsonDocument(createOffer).toJson(QJsonDocument::Compact);
            if(onlineMode)
                sendToNetWorker(byteArr+'\n',distributedHostNum);
            else
                saveOrientedFile(hostName,byteArr);
        }
        else if(type=="sdp"||type=="candidate")
        {
            int targetHostNum=msg["target"].toInt();
            if(targetHostNum==1)
            {
                if(type=="sdp")
                    emit goCreateLocalAnswerER(msg["hostname"].toString(),msg["source"].toInt(),msg["sdp"].toString(),msg["index"].toInt());
                else if(type=="candidate")
                    emit goSetCandidate(msg["candidateItem"].toString(),msg["candidateMid"].toString(),msg["source"].toInt(),msg["index"].toInt());
            }
            else
            {
                QByteArray byteArr=QJsonDocument(msg).toJson(QJsonDocument::Compact);
                if(onlineMode)
                    emit sendToNetWorker(byteArr+'\n',targetHostNum);
                else
                    saveOrientedFile(hostNameVec[msg["target"].toInt()],byteArr);
            }
        }
    }

signals:
    void sendToNetWorker(const QByteArray&,int);
};

#endif // COORJSONWORKER_H
