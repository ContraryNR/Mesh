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
#include <QJsonObject>
#include <QJsonDocument>
#include "peernetworker.h"
#include "coornetworker.h"
#include "dcmanager.h"
#include "tunloader.h"
#include "tunmanager.h"
#include "tuninworker.h"
#include "tunoutworker.h"
#include "ui_mainwindow.h"
#include "filesender.h"
#include "videochatwindow.h"
#include "audiochatwindow.h"
#include "coorjsonworker.h"
#include "peerjsonworker.h"
#include "jsonloader.h"
#include "videoencoder.h"
#include "videodecoder.h"
#include "audiocapture.h"
#include "audioencoder.h"
#include "audiodecoder.h"
#include "audioplayer.h"
#include "netconfig.h"
#include <QTime>

#define NET 0
#define DC 1
#define TIN 2
#define TOUT 3
#define JS 4
#define EN 5
#define DE 6
#define AU 7

//避免使用像"IN","OUT","maxSize"这样的'语义宽泛'的define
//本例和Qt(/windows)自带define冲突导致宏定义覆盖=>头文件级联报错

#define trdAmount 8

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
    coornetworker* serverNetWorker{NULL};
    peernetworker* clientNetWorker{NULL};
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
    QThread* trd[8]{nullptr};//原 7 个索引 + 给音频预留 1 个(capture/player/encoder 共用)
//索引:NET=0 DC=1 TIN=2 TOUT=3 JS=4 EN=5 DE=6 AU=7

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
    //(1)callParams: dcworker 透传上来的 QJsonObject(receiver 业务侧按 msgType 自取字段,含 explain)
    //(2)voidDCWorker: worker 指针,accept 后 mainwindow 直接给它的 std::atomic 成员写协商参数
    //(3)explain 不再单独作为形参,统一从 callParams.value("explain").toString() 读
    void onTransferRequest(uint8_t msgType,uint64_t requestTime,const QJsonObject& callParams,void* voidDCWorker);
public:
    //(1)wire 格式:TYPE_NEGOTIATE(1) + neogotieteType(1) + neogotiateSate(1) + requestTime(8) + json(N)
    //(2)json 包含 request 对应信息(由 mainwindow 业务侧按 type 决定填哪些字段):
    //     - explain:人类可读说明
    //     - 视频通话字段:videoWidth/Height/Fps
    //     - 音频通话字段:audioSampleRate/audioChannelCount(video 通话也会带)
    //     - 文件传输字段:fileName/fileSize
    //(3)requestTime 是 8 字节 wire 字段(不进 json),receiver 解析后透传给 mainwindow
    //(4)响应方向(createResponse)wire 格式不变
    QByteArray createRequest(uint8_t type,uint64_t requestTime,const QJsonObject& json)
    {
        QByteArray request;
        request.append((char)TYPE_NEGOTIATE);
        request.append((const char*)&type,1);
        request.append((char)(10));
        request.append((const char*)(&requestTime),8);
        request.append(QJsonDocument(json).toJson(QJsonDocument::Compact));
        return request;
    }
    QByteArray createResponse(uint8_t type,bool accept,uint64_t requestTime)
    {
        QByteArray response;
        response.append((char)TYPE_NEGOTIATE);//协商类型前缀
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
public://audioSessionReSources
    //(1)音频通话现在也提供独立弹窗(AudioChatWindow,类似"音乐播放器"风格,小尺寸)
    //(2)这是为了避免仅音频通话时用户无法主动挂断 -> 在某些路径(如"停止组网")
    //   上无法经过正常挂断流程直接 release audio pipeline,导致崩溃
    //(3)peerHostNum -> 弹窗指针,一对一(目前设计为同时只允许一路音频会话)
    //(4)audio pipeline 三件套(capture/encoder/player) 与视频共用同一 trd[AU](=trd[7])
    QHash<int, AudioChatWindow*> audioChatWindows;
    QSet<int> audioChatSessions;//当前正在进行的音频通话对方 hostNum(等价于 audioChatWindows 的 keys)
    audiocapture*  audioCap   {nullptr};
    audioencoder*  audioEnCoder{nullptr};
    audiodecoder*  audioDeCoder{nullptr};//receiver 端首次收到音频帧时由 dcworker 启动
    audioplayer*   audioPlayer{nullptr};
    int            audioCalleeHostNum{0};//当前正在进行的音频通话对方 hostNum(用于精确 release)
    void closeAudioChatSession(int peerHostNum);//挂断某路音频会话(本地资源释放 + 通知对方)
    void cleanupAudioChatPipeline();//仅本地释放 audioCap/Encoder/Player/thread,不清 isAudioCalling

public://callNegotiationParams(注意:不作为 mainwindow 成员,而是 onStart* / onTransferRequest 内的 stack 局部变量)
    //(1)发起方:在 onStartVideoChat / onStartAudioChat 弹窗内 pop 出,作为本地变量
    //(2)接收方:在 onTransferRequest 弹窗内 accept 后,直接给 worker 写 std::atomic<int> 成员(无需 invokeMethod)
    //(3)所以 mainwindow 不再持有 pendingXxx 字段,避免跨会话状态污染

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
    //(1)弹窗变体:question 用粗体(显眼),params 用等宽字体(正常显示)
    //(2)返回 true = 用户同意;false = 拒绝或关闭
    bool requestDialogRich(const QString& title,const QString& question,const QString& paramsText,
                           const QString& btnText1,const QString& btnText0);
    //通话请求弹窗:显示对方参数 + 本端可编辑参数 + 音频设备选择
    bool requestDialogWithDevice(const QString& title,const QString& question,const QString& paramsText,
                                  int msgType,int& outVw,int& outVh,int& outVf,int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate);
    netConfig getCoordinateNetConfigFromUI();
    netConfig getPeerNetConfigFromUI();
    bool popVideoCallParamsDialog(int& outW,int& outH,int& outFps,int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate);
    bool popAudioCallParamsDialog(int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate);

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
    //(1)vw/vh/vf:视频参数(传给 videoencoder)
    //(2)asr/acc:音频参数(视频通话内嵌音频,所以同时协商)
    //(3)audioDev:音频输入设备
    void confirmStartVideoChat(int callingHostNum,int videoWidth,int videoHeight,int videoFps,
                               int audioSampleRate,int audioChannelCount,const QAudioDevice& audioDev,int noiseGate=2);
    void onEndVideoChat(int peerHostNum);
    void onRemoteVideoHangup(int peerHostNum);
    void cleanupVideoChatSession(int peerHostNum);
    void closeVideoChatWindow();

public slots://audioSlot
    void onStartAudioChat();
    void confirmStartAudioChat(int callingHostNum,int audioSampleRate,int audioChannelCount,const QAudioDevice& audioDev,int noiseGate=2);
    void onEndAudioChat(int peerHostNum);//主动挂断
    void onRemoteAudioHangup(int peerHostNum);//对方挂断
    void cleanupAudioChatSession(int peerHostNum);
    void onDecodedAudioFlood(const QByteArray& pcmData,int peerHostNum);//订阅 dcmanager::transferDecodedAudio
    //(1)音频弹窗(以及视频弹窗)的"静音"按钮回调 -> 转发给 audiocapture::setMuted
    //   - muted=true:本端停止向对端送 PCM
    //   - 调用方传入 callingPeerHostNum,用于多路复用音频时的目标选择
    //   - 视频弹窗的 mute 也会走到这里(共用同一 audioCap)
    void onAudioChatMuteToggled(int callingPeerHostNum,bool muted);
    //(1)从 audiodecoder::sendDecodedAudioLevel 透传过来的对端音量,推到对应 peer 的 AudioChatWindow
    //   - 视频通话也共用这个 slot(把音量画在视频弹窗的某处;目前只画音频弹窗)
    void onRemoteAudioLevel(int level,int peerHostNum);
    //(1)从 audiocapture::sendPcmLevel 透传过来的本端音量,推到对应 peer 的弹窗
    void onLocalAudioLevel(int level,int peerHostNum);
signals:
    //主线程的桥接信号,由 onDecodedAudioFlood 触发,在主线程收到后转发到 trd[AU] 上的 audioplayer
    //(因为 dcmanager 在 trd[DC],跨线程 QueuedConnection 是默认行为)
    void inboundDecodedAudio(const QByteArray& pcmData);

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
