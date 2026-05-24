#ifndef FILESENDER_H
#define FILESENDER_H
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include "dcmanager.h"

#define chunkSize 23592
//busySize=104857 => remainSize=26214 =*0.9=>23592(23KB)(剩2.56KB)
//确保dc缓冲区/就差一点(例如就差1b)达到busySize/时再进一个chunkPackage不会爆

class filesender : public QObject
{
    Q_OBJECT
public:
    QString filepath;
    std::atomic<bool> running{false};
    filesender(const QString& file):filepath(file){}
public:
    QByteArray chunkPacker(uint32_t chunkIndex,uint32_t chunkAmount,uint8_t fileNameLength,const QByteArray& fileName,const QByteArray& data)
    {
        
        QByteArray msg;
        msg.append(static_cast<char>(0x01));
        msg.append(reinterpret_cast<const char*>(&chunkIndex), sizeof(chunkIndex));
        msg.append(reinterpret_cast<const char*>(&chunkAmount), sizeof(chunkAmount));
        msg.append(static_cast<char>(fileNameLength));
        msg.append(fileName.left(fileNameLength));
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
        int chunkIndex = 0;
        int fileSize=file.size(),mainChunkAmount=fileSize/chunkSize;
        int chunkAmount=mainChunkAmount+((fileSize-mainChunkAmount*chunkSize)>0?1:0);
        QString fileName = QFileInfo(filepath).fileName();
        QByteArray fileNameByteArr = fileName.toUtf8();
        uint8_t fileNameBytes=fileNameByteArr.size();

        qDebug() << "开始发送文件:" << fileName << "文件大小:" << fileSize 
                 << "块大小:" << chunkSize << "总块数:" << chunkAmount;
        
        while(!file.atEnd()&&running)
        {
            QMetaObject::invokeMethod(manager,"sendByteArrayToPeer",Qt::QueuedConnection,Q_ARG(int,targetHostNum),
                                      Q_ARG(const QByteArray&,chunkPacker(chunkIndex,chunkAmount,fileNameBytes,fileNameByteArr,file.read(chunkSize))),
                                      Q_ARG(bool,chunkIndex!=chunkAmount-1));
                                    //chunkIndex==chunkAmount-1 => 当前块为最后的块 => 发送完本chunk后暂无下个块待发 => false
                                    //相反 != 则是只要不是最后一个chunk则预测'有'(true)下个event
            chunkIndex++;
        }
        qDebug() << "文件发送完成，已发送" << chunkIndex << "个块";
        emit fileSendFinish();
    }
signals:
    void fileSendFinish();

signals:
};

#endif // FILESENDER_H
