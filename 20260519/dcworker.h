#ifndef DCWORKER_H
#define DCWORKER_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <string>
#include <variant>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include "rtc/rtc.hpp"

class dcworker : public QObject
{
    Q_OBJECT
public:
    bool isOfferER;int peerHostNum;
    std::shared_ptr<rtc::PeerConnection> pc{NULL};
    std::shared_ptr<rtc::DataChannel> dc{NULL};
    QTimer* sendTimer{NULL},*detectTimer{NULL};
    std::atomic<bool> peerAlive{false},dcValid{false};
    std::vector<rtc::binary>& inboundBuffer;
    QMutex* mutex{NULL};
    std::atomic<bool> isShuttingDown{false};
    dcworker(bool identity,int peerNum,std::vector<rtc::binary>& inBuffer,QMutex* mtx):isOfferER(identity),peerHostNum(peerNum),inboundBuffer(inBuffer),mutex(mtx){}
public://toolFunction
    QJsonObject offerPacker(const QString& sdp)
    {
        QJsonObject offer;
        offer["type"]="sdp";
        offer["sdpType"]="offer";
        offer["sdp"]=sdp;
        offer["target"]=peerHostNum;
        return offer;
    }

    QJsonObject answerPacker(const QString& sdp)
    {
        QJsonObject answer;
        answer["type"]="sdp";
        answer["sdpType"]="answer";
        answer["sdp"]=sdp;
        answer["target"]=peerHostNum;
        return answer;
    }

    QJsonObject candidatePacker(const QString& candidate,const QString& mid)
    {
        QJsonObject candidateJson;
        candidateJson["type"]="candidate";
        candidateJson["candidateItem"]=candidate;
        candidateJson["candidateMid"]=mid;
        candidateJson["target"]=peerHostNum;
        return candidateJson;
    }

public slots:
    void shutdown()
    {
         if (isShuttingDown.exchange(true)) return;

        if(sendTimer)
         {
             if(sendTimer->isActive())
                 sendTimer->stop();
             sendTimer->deleteLater();
             sendTimer=nullptr;
        }
        if(detectTimer)
        {
            if(detectTimer->isActive())
                detectTimer->stop();
            detectTimer->deleteLater();
            detectTimer=nullptr;
        }
        if(dc)
        {
            dc->onMessage(nullptr);
            dc->onOpen(nullptr);
            dc->onClosed(nullptr);
        }
        if (dc)
        {
            dc->close();
            dc.reset();
        }
        if (pc)
        {
            pc->close();
            pc.reset();
        }
        emit dcFinish();
    }
    void createDc()
    {
        rtc::Configuration config;
        config.iceServers.emplace_back("stun:stun.l.google.com:19302");
        pc= std::make_shared<rtc::PeerConnection>(config);
        pc->onLocalDescription([this](rtc::Description sdp) {
            if(isOfferER)
                emit transferMsg(offerPacker(QString::fromStdString(std::string(sdp))));
            else
                emit transferMsg(answerPacker(QString::fromStdString(std::string(sdp))));
        });
        pc->onLocalCandidate([this](rtc::Candidate candidate) {
            emit transferMsg(candidatePacker(QString::fromStdString(std::string(candidate)),QString::fromStdString(candidate.mid())));
        });
        if(isOfferER)
        {
            dc=pc->createDataChannel("P2PConnection");
            dc->onOpen([this](){
                QMetaObject::invokeMethod(this,"vade",Qt::QueuedConnection);
                dcValid=true;
                dc->onMessage([this](std::variant<rtc::binary, rtc::string> message) {
                    peerAlive.store(true, std::memory_order_relaxed);
                    if (std::holds_alternative<std::string>(message))
                    {
                        QString msg=QString::fromStdString(std::get<std::string>(message));
                        if(msg!=QString("hb")) {
                            emit receiveStringMsg(peerHostNum, msg);
                        }
                    }
                    else
                    {
                        rtc::binary data = std::get<rtc::binary>(message);
                        int size=data.size();
                        if (!size) return;
                        emit sendInboundSpeed(size);
                        QMutexLocker locker(mutex);
                        inboundBuffer.emplace_back(data.begin(),data.end());
                    }
                });
                dc->onClosed([this](){
                    dcValid=false;
                    QMetaObject::invokeMethod(this, "shutdown", Qt::QueuedConnection);
                });

            });
        }
        else
        {
            pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> incoming) {
                dc=incoming;
                dc->onOpen([this](){
                    QMetaObject::invokeMethod(this,"vade",Qt::QueuedConnection);
                    dcValid=true;
                    dc->onMessage([this](std::variant<rtc::binary, rtc::string> message){
                        peerAlive.store(true, std::memory_order_relaxed);
                        if (std::holds_alternative<std::string>(message))
                        {
                            QString msg=QString::fromStdString(std::get<std::string>(message));
                            if(msg!=QString("hb")) {
                                emit receiveStringMsg(peerHostNum, msg);
                            }
                        }
                        else
                        {
                            rtc::binary data = std::get<rtc::binary>(message);
                            int size=data.size();
                            if (!size) return;
                            emit sendInboundSpeed(size);
                            QMutexLocker locker(mutex);
                            inboundBuffer.emplace_back(data.begin(),data.end());
                        }
                    });
                    dc->onClosed([this](){
                        dcValid=false;
                        QMetaObject::invokeMethod(this, "shutdown", Qt::QueuedConnection);
                    });
                });
            });
        }
    }
    void setRemoteSdp(const QString& sdp,const QString& sdpType) {
        if (pc && !sdp.isEmpty())
        {
            auto currentState = pc->signalingState();
            if((sdpType == "answer" && currentState == rtc::PeerConnection::SignalingState::HaveLocalOffer)
                ||(
                sdpType == "offer" && currentState == rtc::PeerConnection::SignalingState::Stable))
            {
                rtc::Description description=rtc::Description(sdp.toStdString(),((sdpType == "offer") ?rtc::Description::Type::Offer:rtc::Description::Type::Answer));
                pc->setRemoteDescription(description);
                if(description.type()==rtc::Description::Type::Offer)
                    pc->setLocalDescription();
            }
        }
    }
    void receiveCandidate(const QString &sdp, const QString &mediaType){
        if(pc&&!sdp.isNull()&&!mediaType.isNull())
            pc->addRemoteCandidate(rtc::Candidate(sdp.toStdString(), mediaType.toStdString()));
    };
    void sendMsg(const QString& Msg)
    {
        if (!dc || !dc->isOpen()) return;
        try
        {
            dc->send(Msg.toStdString());
        } catch (const std::exception& e) {
            qWarning() << "dc send failed:" << e.what();
            dcValid = false;
        }
    }
    void vade()
    {
        if(dc&&dc->isOpen())
        {
            sendTimer=new QTimer(this);
            sendTimer->setInterval(1500);
            connect(sendTimer,&QTimer::timeout,[this](){
                sendMsg("hb");
            });
            sendTimer->start();
            detectTimer=new QTimer(this);
            detectTimer->setInterval(3000);
            connect(detectTimer,&QTimer::timeout,[this](){
                if(!peerAlive)
                {
                    dcValid=false;
                    dc->onMessage(nullptr);
                    sendTimer->stop();
                    detectTimer->stop();
                    shutdown();
                }
                else
                    peerAlive=false;
            });
            detectTimer->start();
        }
    }
signals:
    void dcFinish();
    void transferMsg(const QJsonObject&);
    void sendInboundSpeed(int);
    void receiveStringMsg(int peerHostNum, const QString& msg);
};
#endif
