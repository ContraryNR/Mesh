#ifndef BASENETWORKER_H
#define BASENETWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <QJsonArray>

class basenetworker : public QObject
{
    Q_OBJECT
public:
    QString localHostName;int localHostNum;
    basenetworker(const QString& name):localHostName(name){};

public slots:
    void sendMsg(QTcpSocket* socket,const QJsonObject& msg)
    {
        if((!socket)||(!(socket->isValid())))return;
        if(msg["type"].toString()!="hostname")
        {
            QJsonObject tmp(msg);
            tmp["source"]=localHostNum;
            if(msg["type"].toString()=="sdp"&&msg["sdpType"].toString()=="offer")
                tmp["hostname"]=localHostName;
            socket->write(QJsonDocument(tmp).toJson(QJsonDocument::Compact)+'\n');
        }
        else
            socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact)+'\n');
    }
};

#endif // BASENETWORKER_H
