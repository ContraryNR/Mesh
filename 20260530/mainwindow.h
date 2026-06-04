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
#include "videochatwindow.h"
#include "coorjsonworker.h"
#include "peerjsonworker.h"
#include "jsonloader.h"
#include "videoencoder.h"
#include "settingsdialog.h"
#include "startupdialog.h"
#include "netconfig.h"
#include <QTime>

#define NET 0
#define DC 1
#define IN 2
#define OUT 3
#define JS 4
#define EN 5
#define DE 6

#define trdAmount 7

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
    jsonloader* jsonLoader{NULL};

public://WorkerInQThread
    servernetworker* serverNetWorker{NULL};
    clientnetworker* clientNetWorker{NULL};
    coorjsonworker* coorJsonWorker{NULL};
    peerjsonworker* peerJsonWorker{NULL};
    dcmanager* dcManager{NULL};
    tuninworker* tunInWorker{NULL};
    tunoutworker* tunOutWorker{NULL};
    videoencoder* enCoder{nullptr};
    videodecoder* deCoder{nullptr};

public://WorkerInTempQThread
    QHash<filesender*,QThread*> fileSenderContanier;

public://QThreadReSources
    QThread* trd[7]{nullptr};

public://Flags
    bool isCoordinator;
    bool onlineMode;
    bool isClosing=false;
    int currentPeerHostNum=0;
    int currentRow=-1;
    uint64_t startTime;

public://mission
    QHash<uint64_t,request> mission;
    uint64_t getRunningTime()
    {return QDateTime::currentMSecsSinceEpoch()-startTime;}
public slots:
    void onReturnResult(uint64_t timePoint,bool accept);
    void onTransferRequest(uint8_t requestFlag,uint64_t requestTime,const QString& explain,void* voidDCWorker);
public:
    QByteArray createRequest(uint8_t type,const QString& explain,uint64_t requestTime)
    {
        QByteArray request;
        request.append((char)1);
        request.append((const char*)&type,1);
        request.append((char)(10));
        request.append((const char*)(&requestTime),8);
        request.append(explain.toUtf8());
        return request;
    }
    QByteArray createResponse(uint8_t type,bool accept,uint64_t requestTime)
    {
        QByteArray response;
        response.append((char)1);//NEGOTIATE类型前缀
        response.append((const char*)&type,1);
        response.append((char)(accept));
        response.append((const char*)(&requestTime),8);
        return response;
    }

public://GlobalOnlyHostFlag
    QString localHostName;
    int localHostNum;

public://TunReSources
    WINTUN_ADAPTER_HANDLE adapter{NULL};
    WINTUN_SESSION_HANDLE session{NULL};

public://InboundSharedBuffer(withGlobalMutex)
    QMutex* mutex;
    std::vector<rtc::binary> inboundBuffer;

public://videoSessionReSources
    QHash<int, QPair<VideoChatWindow*, QThread*>> videoChatSessions;
    void updateVideoChatButtonState();
    void updateCallButtonState();

public://CurrentState(uiGetter)
    netConfig currentNetConf;

public://SyncReSources(basedOnDcWorker)
    QHash<int, QString> peerNames;
    QHash<int, QStringList> peerChatHistory;

public://sharedIpRoute(basedOnDcManager)
    QHash<int,QVector<dcworker*>>* ipRoute{nullptr};

public://toolFunction
    QString getCurrentPeerName();
    QString formatFileSize(qint64 bytes);

public://uiFunction
    void addChatBubble(QListWidget* listWidget, const QString& text, bool isSelf, bool isBroadcast);
    void reloadChatHistory();

public://uiGetterFunction
    bool requestDialog(const QString&,const QString&,const QString&,const QString&);
    netConfig getCoordinateNetConfigFromUI();
    netConfig getPeerNetConfigFromUI();

public://workerStateSlot
    void onWorkerPulse(int ibs, int obs, int pr, const QList<fileDownLoadState>& fileState);

public://uiSetterFunction
    void initialUI();
    void updateFileReceiveTable();
    void updateFileTransferTable();
    
public://closeFunction
    void closeEvent(QCloseEvent*);
    void cleanUp(bool isShutDown);
    void releaseTunResource();

public://workerFunction
    void initialSignaling();
    void initialTun();void getTun();void startTun();

public slots://ipTableSlot
    void onPeerAdded(int peerHostNum, const QString& peerHostName);
    void onPeerRemoved(int peerHostNum);
    void onPeerMsgReceived(int peerHostNum,const QString& msg);
    void onPeerTableClicked(QTableWidgetItem* item);
    void onPeerConnectionAmountChanged(int peerHostNum,int currentConnectAmount);

public slots://messageSlot
    void goSendUnicastMsg();
    void goSendBroadcastMsg();
    
public slots://fileSlot
    void onAttachFile();
    void onFileDownLoadFinish(const QString&,int);
    void onFileDownLoadState(const QList<fileDownLoadState>&);

public slots://videoSlot
    void onDecodedFrame(const QImage&,int);
    void onStartVideoChat();
    void onStartVoiceChat();
    void onEndVideoChat(int peerHostNum);
    void onRemoteVideoHangup(int peerHostNum);
    void cleanupVideoChatSession(int peerHostNum);
    void closeVideoChatWindow();
    void confirmStartVideoChat(int callingHostNum);

public slots://settingsSlot
    void onSettingsClicked();

private:
    Ui::MainWindow *ui;

public:
    //sending file track
    class FileTransferInfo
        {
        public:
            QString fileName;
            uint32_t fileSize;
            QDateTime createTime;
            bool chunkFinished=false;
            bool terminateException=false;
            FileTransferInfo(const QString& name,uint32_t size,const QDateTime& time,bool state):fileName(name),fileSize(size),createTime(time),chunkFinished(state){}
        };
    QHash<int, QVector<FileTransferInfo>> fileTransferHash;
    //receiving file track
    class FileReceiveInfo
        {
        public:
            QString fileName;
            qreal progress;
            FileReceiveInfo(const QString& name, qreal p): fileName(name), progress(p) {}
        };
    QHash<int, QVector<FileReceiveInfo>> fileReceiveHash;

public://basicFunction
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};

#endif // MAINWINDOW_H
