#ifndef FILESENDER_H
#define FILESENDER_H
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include "dcmanager.h"

#define chunkSize 25600//dc缓冲区占用达75%时至多剩下32KB 这里最多给到25KB确保不会溢出(缓冲区溢出时抛出异常)

class filesender : public QObject
{
    Q_OBJECT
public:
    QString filepath;
    filesender(const QString& file):filepath(file){}
public:
/*[type:1byte]=>uint8标识rtc::binary消息类型
例如,其中0x00为来自wintun出站发送的消息,0x01为文件发送消息
[index:4bytes]=>uint32标识本消息内’data块’的序号
[totalChunks:4bytes]=>uint32标识本次传输(该文件)的总块数
[fileNameLen:1byte]=>uint8(最大255)标识后面紧跟的fileName字节数
[fileName:Nbytes]=>文件名可变字符串(0~255Bytes)
[data]=>实际数据*/
    QByteArray filePacker(uint32_t index,uint32_t chunkAmount,uint8_t fileNameLength,const QString& filename,const QByteArray& data)
    {
        
        QByteArray msg;
        uint8_t flag=0x01;
        msg.append(static_cast<char>(flag));
        msg.append(reinterpret_cast<const char*>(&index), sizeof(index));
        msg.append(reinterpret_cast<const char*>(&chunkAmount), sizeof(chunkAmount));
        msg.append(static_cast<char>(fileNameLength));
        msg.append(filename.toUtf8());
        msg.append(data);
        return msg;
    }
public slots:
    void sendFile(void* voidDcManager,int targetHostNum)
    {
        dcmanager* manager=(dcmanager*)voidDcManager;
        QFile file(filepath);
        if(!file.open(QFile::ReadOnly))
            return;
        int index = 0;
        int chunkAmount = (file.size() + chunkSize - 1) / chunkSize;
        QString fileName = QFileInfo(filepath).fileName();
        while(!file.atEnd())
        {
            QByteArray singleChunk = file.read(chunkSize);
            QMetaObject::invokeMethod(manager,"sendByteArrayToPeer",Qt::QueuedConnection,Q_ARG(int,targetHostNum),
                                      Q_ARG(const QByteArray&,filePacker(index,chunkAmount,fileName.size(),fileName,singleChunk)),Q_ARG(bool,singleChunk!=chunkAmount));
            index++;
        }
        emit fileSendFinish();
    }
signals:
    void fileSendFinish();

signals:
};

#endif // FILESENDER_H
