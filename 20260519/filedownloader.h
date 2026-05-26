#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QHash>
#define CHUNK_SIZE 23592

class filedownloader : public QObject
{
    Q_OBJECT
public:
    QFile* file;
    QString fileName;
    uint64_t writedChunkAmount=0,totalChunkAmount=0;
    std::atomic<bool> running=false;
    filedownloader(const QString& filename,uint64_t chunkAmount):file(new QFile(QDir::currentPath() + "/" + filename)),fileName(filename)
    {
        totalChunkAmount=chunkAmount;
        if (!file->open(QIODevice::WriteOnly))
            qDebug()<<"文件打开失败";
    }
public slots:
    void writeToChunkIndex(uint64_t chunkIndex,const QByteArray& chunk)
    {
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
        file->write(chunk);
        writedChunkAmount++;
        if(writedChunkAmount==totalChunkAmount)
        {
            file->close();
            emit fileWriteFinished(fileName);
            running=false;
        }
    }
signals:
    void fileWriteFinished(const QString& filename);
};

#endif // FILEDOWNLOADER_H
