#ifndef SERVERNETWORKER_H
#define SERVERNETWORKER_H

#include "basenetworker.h"

class servernetworker : public basenetworker
{
    Q_OBJECT

public:
    QTcpServer* server{NULL};
    QVector<QString> hostNameVec;
    QVector<QTcpSocket*> hostSocketVec;
    QHash<QTcpSocket*, QByteArray> socketBuffers;
    servernetworker(const QString& localHostName):basenetworker(localHostName)
    {localHostNum=1;}

public:
    QTcpSocket* getSocket(int hostNum)
    {
        if(hostNum>=2)
            return hostSocketVec[hostNum-2];
        else
            return nullptr;
    }
    void addPeer(const QString& currenthHostName,QTcpSocket* currentHostSocket)
    {
        bool ifExist=false;int distributedHostNum;
        QJsonArray pendingHostName,pendingHostNum;
        pendingHostName.append(localHostName);
        pendingHostNum.append(1);
        for(auto beg=hostNameVec.begin();beg<hostNameVec.end();beg++)
            if(*beg!=currenthHostName)
            {
                ifExist=false;
                if(!(hostSocketVec[beg-hostNameVec.begin()]))
                    continue;
                pendingHostName.append(*beg);
                pendingHostNum.append(beg-hostNameVec.begin()+2);
            }
            else
            {
                ifExist=true;
                hostSocketVec[beg-hostNameVec.begin()]=currentHostSocket;
                distributedHostNum=beg-hostNameVec.begin()+2;
            }
        if(!ifExist)
        {
            hostNameVec.append(currenthHostName);
            hostSocketVec.append(currentHostSocket);
            currentHostSocket=nullptr;
            distributedHostNum=hostNameVec.size()-1+2;
        }
        informNewPeerCreateOffer(pendingHostName,pendingHostNum,distributedHostNum);
    }
    void informNewPeerCreateOffer(const QJsonArray& hostNameArr,const QJsonArray& hostNumArr,int distributedHostNum)
    {
        QJsonObject createOffer;
        createOffer["type"]="createOffer";
        createOffer["yourHostNum"]=distributedHostNum;
        createOffer["hostNameList"]=hostNameArr;
        createOffer["hostNumList"]=hostNumArr;
        sendMsg(getSocket(distributedHostNum),createOffer);
    }
public slots:
    void transferWorkerMsg(const QJsonObject& obj)
    {
        sendMsg(getSocket(obj["target"].toInt()),obj);
    }
    void onReadyRead(QTcpSocket* socket)
    {
        QByteArray& buf = socketBuffers[socket];
        buf += socket->readAll();
        while (buf.contains('\n'))
        {
            int index = buf.indexOf('\n');
            QByteArray oneMsg = buf.left(index);buf.remove(0, index + 1);
            QJsonDocument doc= QJsonDocument::fromJson(oneMsg);
            if (doc.isObject())
            {
                QJsonObject obj=doc.object();
                QString type = obj["type"].toString();
                if(type=="hostname")
                    addPeer(obj["hostname"].toString(),socket);
                else if(type=="sdp"||type=="candidate")
                {
                    QTcpSocket* socket=getSocket(obj["target"].toInt());
                    if(socket&&socket->isValid())
                        socket->write(oneMsg+'\n');
                    else
                    {
                        if(type=="sdp")
                            emit goCreateLocalAnswerER(obj["hostname"].toString(),obj["source"].toInt(),obj["sdp"].toString());
                        else if(type=="candidate")
                            emit goSetCandidate(obj["candidateItem"].toString(),obj["candidateMid"].toString(),obj["source"].toInt());
                    }
                }
            }
        }
    }
    void startTcpServer(const QString& ip,int port)
    {
        if(!server)
        {
            server=new QTcpServer(this);
            connect(server,&QTcpServer::newConnection,[this](){
                QTcpSocket* socket = server->nextPendingConnection();
                connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
                    onReadyRead(socket);
                });
                connect(socket,&QTcpSocket::disconnected,this,[this,socket](){
                    socket->deleteLater();
                    int index = hostSocketVec.indexOf(socket);
                    if(index >= 0)
                        hostSocketVec[index] = nullptr;
                    socketBuffers.remove(socket);
                });
            });
        }
        if(server->isListening())
            server->close();
        server->listen(QHostAddress(ip),port);
    }
    void pauseTcpServer()
    {
        if(server)
        {
            if(server->isListening())
                server->close();
            for(QTcpSocket* socket : server->findChildren<QTcpSocket*>())
                if(socket->isValid())
                    socket->disconnectFromHost();
        }
    }
signals:
    void goCreateLocalAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer);
    void goSetCandidate(const QString& candidate,const QString& mid,int peerHostNum);
};

#endif // SERVERNETWORKER_H
