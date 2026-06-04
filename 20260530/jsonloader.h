#ifndef JSONLOADER_H
#define JSONLOADER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

class jsonloader : public QObject
{
    Q_OBJECT
public slots:
    void loadJsonFile(const QString& filePath)
    {
        QFile file(filePath);
        if(!file.open(QIODevice::ReadOnly))
        {
            qWarning() << "无法打开json文件:" << filePath;
            emit loadFailed(filePath, "无法打开文件");
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if(error.error != QJsonParseError::NoError)
        {
            qWarning() << "json解析失败:" << error.errorString();
            emit loadFailed(filePath, error.errorString());
            return;
        }

        if(doc.isObject())
        {
            emit jsonObjLoaded(doc.object());
            emit loadSuccess(filePath);
        }
        else
            emit loadFailed(filePath, "json不是对象格式");
    }

signals:
    //updateUi
    void loadSuccess(const QString& filePath);
    void loadFailed(const QString& filePath, const QString& errorMsg);
    void jsonObjLoaded(const QJsonObject& jsonObj);
    
};

#endif // JSONLOADER_H
