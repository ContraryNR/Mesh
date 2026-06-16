#ifndef COORNETWORKER_H
#define COORNETWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QJsonArray>
#include <QDebug>
#include "logger.h"

class coornetworker : public QObject
{
    Q_OBJECT

public:
    QTcpServer* server{NULL};
    QHash<int,QTcpSocket*> hostSocketMap;
    QHash<QTcpSocket*, QByteArray> socketBuffers;
    QString& localHostName;int& localHostNum;
    coornetworker(QString& localhostname,int& localhostnum):localHostName(localhostname),localHostNum(localhostnum){};

public:
    // targetHostNum=1 表示 Coordinator 自身,无需 socket
    QTcpSocket* getSocket(int targetHostNum)
    {
        if(targetHostNum<=1)
            return nullptr;
        return hostSocketMap.value(targetHostNum,nullptr);
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
                QJsonObject msg=doc.object();
                int targetHostNum=msg["target"].toInt();
                if(targetHostNum==1)
                    emit onJsonMsg(msg,socket);
                else
                {
                    QTcpSocket* targetSocket=getSocket(targetHostNum);
                    if(targetSocket && targetSocket->isValid())
                        targetSocket->write(oneMsg+'\n');
                }
            }
        }
    }

public slots:
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
                    // socket 断开时,清理其在 hostSocketMap 中的映射
                    // (coorjsonworker 会基于 hostname 重新建立映射,所以此处只需从 map 移除)
                    int oldKey = hostSocketMap.key(socket, 0);
                    if(oldKey != 0)
                        hostSocketMap.remove(oldKey);
                    socket->deleteLater();
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
    void sendMsg(const QByteArray& msg,int targetHostNum)
    {
        QTcpSocket* socket=getSocket(targetHostNum);
        if(socket&&socket->isValid())
        {
            socket->write(msg);
            socket->flush();
        } else {
            qWarning() << "[SERVER] sendMsg: socket is invalid or null!";
        }
    }

signals:
    void onJsonMsg(const QJsonObject&,QTcpSocket* socket);
};

#endif // COORNETWORKER_H
