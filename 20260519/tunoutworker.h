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
    std::atomic<bool> floodFinish=false;
    tunoutworker(tunloader* loader):functionLoader(loader){}

public slots:
    void startExternalSessionFlood(void* voidSession,void* voidRoute)
    {
        floodFinish=false;
        WINTUN_SESSION_HANDLE session=(WINTUN_SESSION_HANDLE)voidSession;
        QHash<int, dcworker*>* route=(QHash<int, dcworker*>*)voidRoute;
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
                    dcworker* worker = route->value(hostNum, nullptr);
                    if (worker && worker->dc && worker->dc->isOpen())
                    {
                        try
                        {
                            //(1)先构造装载binaryMsg的byeArr
                            QByteArray msg(reinterpret_cast<const char*>(packet),packetSize);
                            uint8_t flag=0x00;//从连续的byte内存构造byteArr->头插标志位=>两步得到拼装串
                            msg.push_front(flag);
                            //(2)立即释放对应出站packet(的内存)并尝试获取下个packet*
                            functionLoader->ReleaseReceivePacket(session, packet);
                            packet = functionLoader->ReceivePacket(session, &packetSize);
                            worker->newEventNow=true;//通知QTimer中断处理积压数据包
                            //(3)根据packet*是否非空判断出(极)短时间内是否还有下个packet待invoke异步投递发送事件
                            QMetaObject::invokeMethod(worker,"sendBinaryMsg",Qt::QueuedConnection,Q_ARG(const QByteArray&,msg),Q_ARG(bool,packet!=nullptr));//为避免'严格'类型匹配意外故这里加上'非空判断'
                        }
                        catch (const std::exception& e)
                        {
                            qWarning() << "dc send failed to host"<< hostNum << ":" << e.what();
                        }
                    }
                }
            }
        }
        floodFinish=true;
    }
};

#endif // TUNOUTWORKER_H
