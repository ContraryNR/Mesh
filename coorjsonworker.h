#ifndef COORJSONWORKER_H
#define COORJSONWORKER_H

#include "basejsonworker.h"
#include <QSet>
#include <QJsonArray>
#include <QDebug>
#include <QTcpSocket>
#include "util.h"

class coorjsonworker : public basejsonworker
{
    Q_OBJECT
public:
    // hostName -> hostNum (确定性映射:同一 hostName 必然得到同一 hostNum)
    QHash<QString,int> nameToNumMap;
    // hostNum -> QTcpSocket* (持有 servernetworker 的引用以便反查 socket)
    QHash<int,QTcpSocket*>* numToSocketMap;
    coorjsonworker(QString& localhostname,int& localhostnum,QHash<int,QTcpSocket*>* numToSocketMapPtr,bool onlineRunning,void* voidIpRoute)
        :basejsonworker(localhostname,localhostnum,onlineRunning,voidIpRoute)
    {numToSocketMap=numToSocketMapPtr;}

    // 构造已占用 hostNum 集合 (供冲突解决使用)
    QSet<int> occupiedHostNums() const
    {
        QSet<int> s;
        s.insert(1); // Coordinator 自身
        for(int n : nameToNumMap.values())
            s.insert(n);
        return s;
    }

    // 离线模式需要根据 hostNum 反查 hostName(用于文件命名)
    QString hostNameFor(int hostNum) const
    {
        return nameToNumMap.key(hostNum);
    }

public slots:
    void onInternalMsg(const QJsonObject& msg)
    {
        //不要把getFinal和toJson搞反了啊你这家伙(结果就是反了就没法把sourceHostNum字符封装进去)
        QByteArray byteArr=QJsonDocument(getFinalJson(msg)).toJson(QJsonDocument::Compact);
        int targetHostNum=msg["target"].toInt();
        if(onlineMode)
            emit sendToNetWorker(byteArr+'\n',targetHostNum);
        else
        {
            if(msg["index"].toInt()!=0)
                backSignaling(getFinalJson(msg),getDcWorker(ipRoute,msg["target"].toInt(),TYPE_TUN));
            else
            {
                QString type=msg["type"].toString();
                QString mType=(type=="sdp")?msg["sdpType"].toString():type;
                saveOrientedFile(hostNameFor(targetHostNum),byteArr,mType);
            }

        }
    }
    void onExternalMsg(const QJsonObject& msg,QTcpSocket* socket)
    {
        QString type = msg["type"].toString();
        if(type=="hostname")
        {
            QString hostName=msg["hostname"].toString();
            int baseHostNum=hashHostNameToHostNum(hostName);
            QSet<int> occupied=occupiedHostNums();
            int distributedHostNum=resolveHostNumCollision(baseHostNum,occupied);

            nameToNumMap[hostName]=distributedHostNum;
            if(numToSocketMap && socket)
                (*numToSocketMap)[distributedHostNum]=socket;

            for(auto it=nameToNumMap.begin();it!=nameToNumMap.end();++it)
            {
                if(it.key()==hostName) continue; // 跳过自己
                int existingHostNum=it.value();
                QJsonObject newPeerJson;
                newPeerJson["type"]="newPeer";
                newPeerJson["hostName"]=hostName;
                newPeerJson["hostNum"]=distributedHostNum;
                newPeerJson["target"]=existingHostNum;
                if(onlineMode)
                {
                    QByteArray byteArr=QJsonDocument(newPeerJson).toJson(QJsonDocument::Compact);
                    emit sendToNetWorker(byteArr+'\n',existingHostNum);
                }
                else
                {
                    QByteArray byteArr=QJsonDocument(getFinalJson(newPeerJson)).toJson(QJsonDocument::Compact);
                    saveOrientedFile(it.key(),byteArr,"newPeer");
                }
            }

            emit goCreateOfferER(hostName,distributedHostNum);

            QJsonObject distJson;
            distJson["type"]="distributedHostNum";
            distJson["hostNum"]=distributedHostNum;
            distJson["target"]=distributedHostNum;
            if(onlineMode)
            {
                QByteArray byteArr=QJsonDocument(distJson).toJson(QJsonDocument::Compact);
                emit sendToNetWorker(byteArr+'\n',distributedHostNum);
            }
            else
            {
                QByteArray byteArr=QJsonDocument(getFinalJson(distJson)).toJson(QJsonDocument::Compact);
                saveOrientedFile(hostName,byteArr,"distributedHostNum");
            }
        }
        else if(type=="sdp"||type=="candidate")
        {
            int targetHostNum=msg["target"].toInt();
            if(targetHostNum==1)
            {
                if(type=="sdp")
                {
                    if(msg["sdpType"].toString()=="offer")
                        emit goCreateLocalAnswerER(msg["hostname"].toString(),msg["source"].toInt(),msg["sdp"].toString(),msg["index"].toInt());
                    else if(msg["sdpType"].toString()=="answer")
                        emit goSetAnswer(msg["sdp"].toString(),msg["source"].toInt(),msg["index"].toInt());
                }
                else if(type=="candidate")
                    emit goSetCandidate(msg["candidateItem"].toString(),msg["candidateMid"].toString(),msg["source"].toInt(),msg["index"].toInt());
            }
            else
            {
                QByteArray byteArr=QJsonDocument(msg).toJson(QJsonDocument::Compact);
                //这里target非己逻辑可留可不留 因为对于online模式可以在最上游serverNetWorker即解析无需再封包即可转发
                //对于offline模式 除非Coor用户蠢到能把明确写着"toXXX.json"里面'XXX'写的不是Coordinator的名字导进软件那就也没必要=>也算是增加容错 避免真的有那种傻*
                if(onlineMode)
                    emit sendToNetWorker(byteArr+'\n',targetHostNum);
                else
                {
                    QString mType=(type=="sdp")?msg["sdpType"].toString():type;
                    saveOrientedFile(hostNameFor(targetHostNum),byteArr,mType);
                }
            }
        }
    }

signals:
    void sendToNetWorker(const QByteArray&,int);
    void goCreateOfferER(const QString& peerHostName,int peerHostNum);
    void goCreateLocalAnswerER(const QString& peerHostName,int peerHostNum,const QString& offer,int index);
    void goSetAnswer(const QString& sdp,int peerHostNum,int index);
    void goSetCandidate(const QString& candidate,const QString& mid,int peerHostNum,int index);
    void offlineFileSaved(const QString& hintMsg);
};

#endif // COORJSONWORKER_H
