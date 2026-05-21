#ifndef TUNOUTWORKER_H
#define TUNOUTWORKER_H

#include <QObject>
#include <QCoreApplication>
#include <QPointer>
#include "tunloader.h"
#include "rtc/rtc.hpp"
#include "dcmanager.h"
#define ld load(std::memory_order_relaxed)

class tunoutworker : public QObject
{
    Q_OBJECT
public:
    tunloader* functionLoader{NULL};
    std::atomic<bool> sessionRunning=false;
    QTimer* sendSpeedTimer{NULL};
    std::atomic<int> externalSpeed=0,floodFinish;
    tunoutworker(tunloader* loader):functionLoader(loader)
    {
        if(!sendSpeedTimer)
        {
            sendSpeedTimer=new QTimer(this);
            sendSpeedTimer->setInterval(1000);
            connect(sendSpeedTimer,&QTimer::timeout,this,[this](){
                emit sendExternelSpeed(externalSpeed);
                externalSpeed=0;
            });
        }
    }

signals:
    void sendExternelSpeed(int);

public slots:
    void startExternalSessionFlood(void* voidSession,void* voidRoute)
    {
        floodFinish=false;
        WINTUN_SESSION_HANDLE session=(WINTUN_SESSION_HANDLE)voidSession;
        QHash<int, dcworker*>* route=(QHash<int, dcworker*>*)voidRoute;
        sendSpeedTimer->start();
        HANDLE readEvent = functionLoader->GetReadWaitEvent(session);
        while (sessionRunning.ld)
        {
            if (WaitForSingleObject(readEvent, 100) == WAIT_TIMEOUT)
            {
                QCoreApplication::processEvents();
                continue;
            }
            DWORD packetSize = 0;
            BYTE* packet = functionLoader->ReceivePacket(session, &packetSize);
            while (sessionRunning.ld&& packet)
            {
                if (packetSize >= 20)
                {
                    uint32_t dstAddr;
                    std::memcpy(&dstAddr, packet + 16, sizeof(uint32_t));
                    int hostNum = ntohl(dstAddr) & 0xFF;
                    dcworker* target = route->value(hostNum, nullptr);
                    if (target && target->dc && target->dc->isOpen())
                    {
                        try
                        {
                            target->dc->send(reinterpret_cast<const rtc::byte*>(packet),packetSize);
                            externalSpeed += packetSize;
                        }
                        catch (const std::exception& e)
                        {
                            qWarning() << "dc send failed to host"<< hostNum << ":" << e.what();
                        }
                    }
                }
                functionLoader->ReleaseReceivePacket(session, packet);
                packet = functionLoader->ReceivePacket(session, &packetSize);
            }
        }
        sendSpeedTimer->stop();
        floodFinish=true;
    }
};

#endif // TUNOUTWORKER_H
