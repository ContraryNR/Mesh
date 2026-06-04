#ifndef BASEJSONWORKER_H
#define BASEJSONWORKER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFIle>
#include <QDir>

class basejsonworker : public QObject
{
    Q_OBJECT
public:
    bool onlineMode;
    QString& localHostName;int& localHostNum;
    basejsonworker(QString& localhostname,int& localhostnum,bool onlineRunning=true)
        :localHostName(localhostname),localHostNum(localhostnum),onlineMode(onlineRunning){};

public:
    QJsonObject getFinalJson(QJsonObject msg)
    {
        if(msg["type"].toString()=="hostname")
            msg["hostname"]=localHostName;
        else
        {
            msg["source"]=localHostNum;
            if(msg["type"].toString()=="sdp"&&msg["sdpType"].toString()=="offer")
                msg["hostname"]=localHostName;
        }
        return msg;
    }
    void saveOrientedFile(const QString& orientation,const QByteArray& content)
    {
        QFile jsonFile(QDir::currentPath() + "/" + QString("to") + orientation + QString(".json"));
        if(!jsonFile.open(QIODevice::WriteOnly))
            if(!jsonFile.open(QIODevice::WriteOnly))
                return;
        jsonFile.write(content);
        jsonFile.close();
        emit offlineFileSaved(QString("离线模式: 消息已保存到 to%1.json，请发送给%1").arg(orientation));
    }

signals:
    void transferDcManagerMsgToNetWorker(const QByteArray&);
    void goCreateLocalAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer,int index);
    void goSetCandidate(const QString& candidate,const QString& mid,int peerHostNum,int index);
    void offlineFileSaved(const QString& hintMsg);
};

#endif // BASEJSONWORKER_H
