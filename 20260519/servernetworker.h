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
        //根据hostName判断 此前将对应hostSocket置空没问题
        for(auto beg=hostNameVec.begin();beg<hostNameVec.end();beg++)
            if(*beg!=currenthHostName)
            {
                ifExist=false;
                //<1.3>若现存的hostNameVec中host已离线(socket==nullptr)则不再通报newPeer该离线主机的存在
                if(!(hostSocketVec[beg-hostNameVec.begin()]))
                    continue;//<1.4>将错误的return换为continue 解决新主机试图加入时发现有host离线过就返回导致无法加入的问题
                pendingHostName.append(*beg);//能tm写成return也能证明我的精神状态是有多差了草...
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
        //QHash[key] 指定Key不存在时自动创建一个默认值并插入该键值对->nextPendingSocket返回新socket时无需插入键值对
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
                    if(socket&&socket->isValid())//<1.1>增加isValid()判断 确保当/之前的主机离线后/不会从已经关闭的socket/转发offer
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
                // connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);
                //<1.2>需要增加socket断开后
                //(1)tcp连接后将hostSocketVec对应socket*置空
                //(2)socketBuffer对应键值对删除
                connect(socket,&QTcpSocket::disconnected,this,[this,socket](){
                    socket->deleteLater();
                    int index = hostSocketVec.indexOf(socket);
                    if(index >= 0)//<1.5>在<1.4>的基础上 nextPendingSocket传递新socket(但是disconnect回调是配置好了的)却在试图加入hostSocketVec时返回append失败导致后续获取index为-1
                        hostSocketVec[index] = nullptr;//但奇怪的是报的错的容器类型是QList而不是QVector"ASSERT failure in QList::operator[]: "index out of range""
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
