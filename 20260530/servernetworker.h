#ifndef SERVERNETWORKER_H
#define SERVERNETWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <QJsonArray>
#include <QDebug>
#include "logger.h"

class servernetworker : public QObject
{
    Q_OBJECT

public:
    QTcpServer* server{NULL};

    QVector<QTcpSocket*> hostSocketVec;
    QHash<QTcpSocket*, QByteArray> socketBuffers;
    QString& localHostName;int& localHostNum;
    servernetworker(QString& localhostname,int& localhostnum):localHostName(localhostname),localHostNum(localhostnum){};

public:
    QTcpSocket* getSocket(int targetHostNum)
    {
        if(targetHostNum>=2)
            return hostSocketVec[targetHostNum-2];
        else
            return nullptr;
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
                emit onJsonMsg(doc.object(),socket);
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

#endif // SERVERNETWORKER_H
