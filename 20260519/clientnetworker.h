#ifndef CLIENTNETWORKER_H
#define CLIENTNETWORKER_H

#include "basenetworker.h"

class clientnetworker : public basenetworker
{
    Q_OBJECT
public:
    QByteArray msg;
    QTcpSocket* clientSocket{NULL};
    clientnetworker(const QString& localHostName):basenetworker(localHostName){}

public slots:
    void transferWorkerMsg(const QJsonObject& obj)
    {
        sendMsg(clientSocket,obj);
    }
    void onReadyRead()
    {
        msg += clientSocket->readAll();
        while (msg.contains('\n'))
        {
            int index = msg.indexOf('\n');
            QByteArray oneMsg = msg.left(index);
            msg.remove(0, index + 1);
            QJsonDocument doc;
            if ((doc= QJsonDocument::fromJson(oneMsg)).isObject())
            {
                QJsonObject obj=doc.object();
                QString type = obj["type"].toString();
                if(type=="createOffer")
                {
                    localHostNum=obj["yourHostNum"].toInt();
                    emit hostNumAssigned(localHostNum);
                    QJsonArray hostNameList=obj["hostNameList"].toArray(),hostNumList=obj["hostNumList"].toArray();
                    if(hostNameList.size()==hostNumList.size())
                        emit goCreateOfferER(hostNameList,hostNumList);
                }else if(type=="sdp")
                {
                    if(obj["sdpType"].toString()=="offer")
                        emit goCreateAnswerER(obj["hostname"].toString(),obj["source"].toInt(),obj["sdp"].toString());
                    else if(obj["sdpType"].toString()=="answer")
                        emit goSetAnswer(obj["sdp"].toString(),obj["source"].toInt());
                }else if(type=="candidate")
                    goSetCandidate(obj["candidateItem"].toString(),obj["candidateMid"].toString(),obj["source"].toInt());
            }
        }
    }
    void startTcpClient(const QString& ip,int port)
    {
        if(clientSocket)
            clientSocket->abort();
        if(!clientSocket)
        {
            clientSocket=new QTcpSocket;
            connect(clientSocket,&QTcpSocket::connected,[ip,port,this](){
                connect(clientSocket,&QTcpSocket::readyRead,this,&clientnetworker::onReadyRead);
                connect(clientSocket,&QTcpSocket::disconnected,[this](){
                    clientSocket->deleteLater();
                    clientSocket=0;
                });
                QJsonObject hostNameJson;
                hostNameJson["type"]="hostname";
                hostNameJson["hostname"]=localHostName;
                clientSocket->write(QJsonDocument(hostNameJson).toJson(QJsonDocument::Compact)+'\n');
            });
        }
        clientSocket->connectToHost(ip,port);
    }
    void disConnect()
    {
        if(clientSocket&&clientSocket->isValid())
            clientSocket->disconnectFromHost();
    }


signals:
    void goCreateOfferER(const QJsonArray&,const QJsonArray&);
    void goCreateAnswerER(const QString&,int,const QString&);
    void goSetAnswer(const QString& sdp,int peerHostNum);
    void goSetCandidate(const QString&,const QString&,int);
    void hostNumAssigned(int);
};

#endif // CLIENTNETWORKER_H


