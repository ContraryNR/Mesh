#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QString>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

class Logger
{
public:
    static Logger& instance()
    {
        static Logger instance;
        return instance;
    }

    void init(const QString& fileName)
    {
        QMutexLocker locker(&mutex);
        logFile.setFileName(fileName);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            stream.setDevice(&logFile);
        }
    }

    void log(const QString& msg)
    {
        QMutexLocker locker(&mutex);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        QString formattedMsg = QString("[%1] %2").arg(timestamp, msg);
        
        qDebug().noquote() << formattedMsg;
        
        if (logFile.isOpen())
        {
            stream << formattedMsg << "\n";
            stream.flush();
            logFile.flush();
        }
    }

    void log(const char* msg)
    {
        log(QString::fromUtf8(msg));
    }

    bool isInitialized() const { return logFile.isOpen(); }

private:
    Logger() = default;
    ~Logger()
    {
        if (logFile.isOpen())
            logFile.close();
    }
    
    QFile logFile;
    QTextStream stream;
    QMutex mutex;
};

class FileDebug
{
public:
    FileDebug() : stream(&buffer, QIODevice::WriteOnly) {}
    
    template<typename T>
    FileDebug& operator<<(const T& value)
    {
        stream << value;
        return *this;
    }
    
    FileDebug& operator<<(const QJsonObject& obj)
    {
        stream << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return *this;
    }
    
    FileDebug& operator<<(const QJsonArray& arr)
    {
        stream << QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        return *this;
    }
    
    ~FileDebug()
    {
        stream.flush();
        Logger::instance().log(buffer);
    }
    
private:
    QString buffer;
    QTextStream stream;
};

#define qLog() FileDebug()

#endif // LOGGER_H
