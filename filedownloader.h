#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QHash>
#include <QSet>
#include "logger.h"
#define CHUNK_SIZE 23592

class filedownloader : public QObject
{
    Q_OBJECT
public:
    QFile* file;
    QString fileName;
    uint64_t writedChunkAmount=0,totalChunkAmount=0;
    QSet<uint64_t> writtenChunks;
    //dc.onMsg可能对同一chunk执行多次异步投递<=事实证明是上游代码逻辑问题 不是libdatachannel发神金
    //反复写入同一chunk的同时触发writedChunkAmount自增并提前达到totalChunkAmount
    //=>提前判定下载完成
    std::atomic<bool> running=false;
    filedownloader(const QString& filename,uint64_t chunkAmount):file(new QFile(QDir::currentPath() + "/" + filename)),fileName(filename)
    {
        totalChunkAmount=chunkAmount;
        bool ok=file->open(QIODevice::WriteOnly);
        qLog()<<"[FILE-DL] 构造 file="<<file->fileName()<<" open="<<ok<<" err="<<file->errorString()<<" chunks="<<chunkAmount;
    }
public slots:
    void writeToChunkIndex(uint64_t chunkIndex,const QByteArray& chunk)
    {
        qLog()<<"[FILE-DL] writeToChunkIndex chunk="<<chunkIndex<<" size="<<chunk.size()<<" running="<<running<<" file="<<(file?file->fileName():"null");
        if(running)
        {
            if(file)
            {
                if(!(file->isOpen()))
                    if(!(file->open(QIODevice::WriteOnly)))
                        return;
            }
            else
                return;
        }
        else
        {
            if(file)
                if(file->isOpen())
                {
                    file->close();
                    delete(file);
                    file=nullptr;
                }
            return;
        }
        file->seek(CHUNK_SIZE*chunkIndex);
        qint64 written=file->write(chunk);
        if(writtenChunks.contains(chunkIndex))//使用set避免重复
        {
            qLog()<<"[FILE-DL] 重复chunk="<<chunkIndex<<" 跳过计数";
            return;
        }
        writtenChunks.insert(chunkIndex);
        writedChunkAmount++;
        if(chunkIndex%50==0||writedChunkAmount==totalChunkAmount)
            qLog()<<"[FILE-DL] 写入chunk "<<chunkIndex<<" written="<<written<<" 总进度="<<writedChunkAmount<<"/"<<totalChunkAmount;
        emit fileWriteProgress(fileName,qreal(writedChunkAmount)*100.0/(qreal(totalChunkAmount)));
        //注意这里虽然*100了 数值本身应在0~100的范围 但问题在于如果信号参数用int仍会导致参数类型不匹配
        if(writedChunkAmount==totalChunkAmount)
        {
            file->close();
            emit fileWriteFinished(fileName);
            running=false;
        }
    }
signals:
    void fileWriteFinished(const QString& filename);
    void fileWriteProgress(const QString&,qreal);
};

#endif // FILEDOWNLOADER_H
