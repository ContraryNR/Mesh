#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QInputDialog>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QFrame>
#include <QTime>
#include "clientnetworker.h"
#include "servernetworker.h"
#include "dcmanager.h"
#include "tunloader.h"
#include "tunmanager.h"
#include "tuninworker.h"
#include "tunoutworker.h"
#include "ui_mainwindow.h"
#include "filesender.h"

#define NET 0
#define DC 1
#define IN 2
#define OUT 3

bool inline checkIPv4(const QString & ipStr)
{
    if(ipStr.isEmpty())
        return false;
    QHostAddress ip;
    if(ip.setAddress(ipStr)&&ip.protocol()==QAbstractSocket::IPv4Protocol)
        return true;
    else
        return false;
}

class netConfig{
public:
    QString ip;int port;
    netConfig():ip(),port(0){}
    netConfig(const QString& IP,int PORT):ip(IP),port(PORT){}
    netConfig(const netConfig& oldOne):ip(oldOne.ip),port(oldOne.port){}
    bool isValid(){return checkIPv4(ip)&&(port>0&&port<65535);}
};

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public://WorkerInMainTrd
    tunloader* tunLoader{NULL};
    tunmanager* tunManager{NULL};

public://WorkerInQThread
    basenetworker* netWorker{NULL};
    dcmanager* dcManager{NULL};
    tuninworker* tunInWorker{NULL};
    tunoutworker* tunOutWorker{NULL};
    QThread* trd[4]{};

public://WorkerInTmpQThread
    QHash<filesender*,QThread*> fileSenderContanier;

public://Flags
    bool isCoordinator;
    QString localHostName;
    bool isClosing{false};
    int currentPeerHostNum{0};
    int currentRow{-1};

public://Sources
    WINTUN_ADAPTER_HANDLE adapter{NULL};
    WINTUN_SESSION_HANDLE session{NULL};
    std::vector<rtc::binary> inboundBuffer;
    QMutex* mutex;
    netConfig currentNetConf;
    QHash<int, QString> peerNames;
    QHash<int, QStringList> peerChatHistory;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();;
    void initialUI();
    bool requestDialog(const QString&,const QString&,const QString&,const QString&);

public://uiGetterFunc
    netConfig getCoordinateNetConfigFromUI();
    netConfig getPeerNetConfigFromUI();

public://softwareFunc
    void closeEvent(QCloseEvent*);
    void cleanUp(bool isShutDown);
    void releaseTunResource();

public://workerFunc
    void initialSignaling();
    void initialTun();void getTun();void startTun();

public slots:
    void onPeerAdded(int peerHostNum, const QString& peerHostName);
    void onPeerRemoved(int peerHostNum);
    void onPeerMsgReceived(int peerHostNum,const QString& msg);
    void goSendUnicastMsg();
    void goSendBroadcastMsg();
    void onPeerTableClicked(QTableWidgetItem* item);
    void onAttachFile();

private:
    void addChatBubble(QListWidget* listWidget, const QString& text, bool isSelf, bool isBroadcast);
    void reloadChatHistory();
    QString getCurrentPeerName();

private:
    Ui::MainWindow *ui;

signals:
    void initialTcpServer(const QString&,int);
    void initialTcpClient(const QString&,int);
    void startVPN();
};

#endif // MAINWINDOW_H
