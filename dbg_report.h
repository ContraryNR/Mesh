#ifndef DBG_REPORT_H
#define DBG_REPORT_H

//TRAE-debugger 用的轻量级事件上报器:
//  - 不阻塞调用线程(fire-and-forget)
//  - 用 QTcpSocket 直接发 HTTP/1.1 POST,无 QNetworkAccessManager 依赖
//  - 从 .dbg/<sessionId>.env 读 URL 和 sessionId
//  - 失败静默(本端崩溃或无网不影响业务)
//
// 用法:
//   dbgEvent("H1", "mainwindow_frontend.cpp:1110", "before audioCap shutdown", {{"ptr", "0xabcd"}});

#include <QString>
#include <QMap>
#include <QVariant>
#include <QTcpSocket>
#include <QFile>
#include <QHostAddress>
#include <QDateTime>
#include <QMetaType>

namespace dbg {

inline QString readEnv(const QString& key, const QString& fallback)
{
    QFile f(".dbg/audio-hangup-crash.env");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return fallback;
    const QByteArray content = f.readAll();
    f.close();
    for (const QByteArray& line : content.split('\n')) {
        if (line.startsWith(key.toUtf8() + "=")) {
            return QString::fromUtf8(line.mid(key.length() + 1).trimmed());
        }
    }
    return fallback;
}

inline void dbgEvent(const QString& hypothesisId,
                     const QString& location,
                     const QString& msg,
                     const QMap<QString, QVariant>& data = {})
{
    static QString sUrl  = readEnv("DEBUG_SERVER_URL",  "http://127.0.0.1:7777/event");
    static QString sSess = readEnv("DEBUG_SESSION_ID", "audio-hangup-crash");

    //构造 JSON(手写,避免依赖 QJsonDocument)
    QString body;
    body += "{\"sessionId\":\"" + sSess + "\",";
    body += "\"runId\":\"pre-fix\",";
    body += "\"hypothesisId\":\"" + hypothesisId + "\",";
    body += "\"location\":\"" + location + "\",";
    body += "\"ts\":" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ",";
    body += "\"msg\":\"" + msg.toHtmlEscaped() + "\"";
    if (!data.isEmpty()) {
        body += ",\"data\":{";
        bool first = true;
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            if (!first) body += ",";
            first = false;
            body += "\"" + it.key() + "\":";
            const QVariant& v = it.value();
            switch (v.typeId()) {
                case QMetaType::Int:
                case QMetaType::LongLong:
                    body += QString::number(v.toLongLong()); break;
                case QMetaType::Double:
                    body += QString::number(v.toDouble(), 'g', 12); break;
                case QMetaType::Bool:
                    body += v.toBool() ? "true" : "false"; break;
                default:
                    body += "\"" + v.toString().toHtmlEscaped() + "\""; break;
            }
        }
        body += "}";
    }
    body += "}";

    //解析 URL 中的 host:port + path
    //期望格式:http://127.0.0.1:7777/event
    QString url = sUrl;
    url.remove("http://");
    const int slash = url.indexOf('/');
    const QString hostport = (slash >= 0) ? url.left(slash) : url;
    const QString path = (slash >= 0) ? url.mid(slash) : "/";
    const int colon = hostport.indexOf(':');
    const QString host = (colon >= 0) ? hostport.left(colon) : hostport;
    const quint16 port = (colon >= 0) ? hostport.mid(colon + 1).toUShort() : 80;

    //fire-and-forget,栈上 socket,等异步 connect/write 完自行析构
    QTcpSocket* sock = new QTcpSocket();
    QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    sock->connectToHost(QHostAddress(host), port);
    if (!sock->waitForConnected(200)) {
        delete sock;
        return;
    }

    QByteArray req;
    req += "POST " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: " + hostport.toUtf8() + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + QByteArray::number(body.toUtf8().size()) + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";
    req += body.toUtf8();
    sock->write(req);
    sock->flush();
    sock->disconnectFromHost();
}

} // namespace dbg

#endif // DBG_REPORT_H
