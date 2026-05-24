#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QHash>

class filereceiver : public QObject
{
    Q_OBJECT
public:
    bool running{false};
    QHash<QString,QHash<int,QByteArray>> fileContainer;
public:
    void receiveFile(const QString& fileName,int chunkIndex,int chunkAmount,const QByteArray& chunk)
    {
        if(!running)//循环正在执行(写入文件)则依赖wait()等待循环停止
            return;//但新来的chunk不再被接收
        QHash<int,QByteArray>& singleFileContainer=fileContainer[fileName];
        singleFileContainer.insert(chunkIndex,chunk);
        
        // 添加调试日志
        qDebug() << "收到文件块:" << fileName << "块" << chunkIndex + 1 << "/" << chunkAmount 
                 << "当前块数:" << singleFileContainer.size();
        
        if(singleFileContainer.size()==chunkAmount)
        {
            qDebug() << "开始写入文件:" << fileName << "总块数:" << chunkAmount;
            QFile file(QDir::currentPath() + "/" + fileName);
            if (!file.open(QIODevice::WriteOnly))
            {
                qDebug()<<"文件打开失败";
                return;
            }
            for(int i=0; i<chunkAmount; i++)
            {
                const QByteArray& singlePart = singleFileContainer[i];
                qint64 bytesWritten = file.write(singlePart);
                if (bytesWritten != singlePart.size())
                {
                    qDebug()<<"文件块写入不完整";
                    file.close();
                    return;
                }
            }
            file.close();
            fileContainer.remove(fileName);
            qDebug()<<"文件保存在"+QDir::currentPath() + "/" + fileName;
        }
    }
};

#endif // FILERECEIVER_H
