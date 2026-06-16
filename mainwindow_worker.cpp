#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), mutex(new QMutex)
{
    ui->setupUi(this);
    initialUI();
    
    QString logFileName = QCoreApplication::applicationDirPath() + "/" + localHostName + ".txt";
    Logger::instance().init(logFileName);
    qLog() << "Logger initialized, log file:" << logFileName;
    
    if(isCoordinator)
        localHostNum=1;
    initialSignaling();
    initialTun();

    connect(ui->btnStart, &QPushButton::clicked, this, [this](){
        ui->btnStart->setEnabled(false);
        startTime=QDateTime::currentMSecsSinceEpoch();
        if(isCoordinator)
        {
            if(onlineMode)
            {
                currentNetConf = getCoordinateNetConfigFromUI();
                if(!currentNetConf.isValid())
                {
                    ui->stateMsg->appendPlainText("配置信息不合法");
                    ui->btnStart->setEnabled(true);
                    return;
                }
                QMetaObject::invokeMethod(serverNetWorker, "startTcpServer", Qt::QueuedConnection,
                                          Q_ARG(const QString&, currentNetConf.ip),
                                          Q_ARG(int, currentNetConf.port));
                ui->stateMsg->appendPlainText(QString("协调者监听在 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
            }
            getTun();
        }
        else
        {
            if(onlineMode)
            {
                currentNetConf = getPeerNetConfigFromUI();
                if(!currentNetConf.isValid())
                {
                    ui->stateMsg->appendPlainText("配置信息不合法");
                    ui->btnStart->setEnabled(true);
                    return;
                }
                QMetaObject::invokeMethod(clientNetWorker, "startTcpClient", Qt::QueuedConnection,
                                          Q_ARG(const QString&, currentNetConf.ip),
                                          Q_ARG(int, currentNetConf.port));
                ui->stateMsg->appendPlainText(QString("正在连接协调者 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
            }
            else
            {
                QMetaObject::invokeMethod(peerJsonWorker,"onReadySendHostName",Qt::QueuedConnection);
            }
        }
        ui->btnShut->setEnabled(true);
        ui->btnSettings->setEnabled(true);
        ui->btnLoadJson->setEnabled(!onlineMode);
        //仅限离线模式使用(虽然在线模式导入也是合法的,均为共享jsonWorker及下游链路但不建议混用)
        if(dcManager)
            QMetaObject::invokeMethod(dcManager,"startTimer",Qt::QueuedConnection);
    });

    connect(ui->btnShut, &QPushButton::clicked, this, [this](){
        ui->btnSend->setEnabled(false);
        ui->btnBroadcast->setEnabled(false);
        ui->btnAttach->setEnabled(false);
        ui->btnShut->setEnabled(false);
        ui->btnLoadJson->setEnabled(false);
        ui->btnSettings->setEnabled(false);
        currentRow=-1;
        cleanUp(false);
    });
}

void MainWindow::initialSignaling()
{
    (dcManager = new dcmanager(inboundBuffer, mutex,onlineMode))->moveToThread(trd[DC] = new QThread);
    ipRoute=&(dcManager->ipRoute);
    connect(dcManager, &dcmanager::workerStatePulse, this,&MainWindow::onWorkerPulse);
    connect(dcManager, &dcmanager::peerAdded, this, &MainWindow::onPeerAdded);
    connect(dcManager, &dcmanager::peerRemoved, this, &MainWindow::onPeerRemoved);
    connect(dcManager, &dcmanager::receiveStringMsg, this, &MainWindow::onPeerMsgReceived);
    connect(dcManager,&dcmanager::informFileDownLoadFinish,this,&MainWindow::onFileDownLoadFinish);
    connect(dcManager,&dcmanager::transferDecodedFrame,this,&MainWindow::onDecodedFrame);
    connect(dcManager,&dcmanager::transferDecodedAudio,this,&MainWindow::onDecodedAudioFlood);
    //(新增)对端音频帧解码后的相对音量 -> mainwindow::onRemoteAudioLevel -> 弹窗
    connect(dcManager,&dcmanager::transferDecodedAudioLevel,this,&MainWindow::onRemoteAudioLevel);
    connect(dcManager,&dcmanager::peerConnectionAmountChanged,this,&MainWindow::onPeerConnectionAmountChanged);
    connect(dcManager,&dcmanager::transferRequest,this,&MainWindow::onTransferRequest);
    connect(dcManager,&dcmanager::returnRequestResult,this,&MainWindow::onReturnResult);
    connect(dcManager,&dcmanager::videoHangupReceived,this,&MainWindow::onRemoteVideoHangup);
    connect(dcManager,&dcmanager::audioHangupReceived,this,&MainWindow::onRemoteAudioHangup);

    connect(dcManager,&dcmanager::dcConnected,this,[this](QString hostName){
        ui->stateMsg->appendPlainText(QString("主机'%1'已建立连接").arg(hostName));
        if(!onlineMode)
            ui->stateMsg->appendPlainText("请停止导入该主机相关Json文件");
    });

    trd[DC]->start();

    //考虑到jsonLoader的职责是读取json文件并异步投递到jsonWorker
    //异步投递前的流程耗时极短 故存于主线程运行
    if(!onlineMode)
    {
        jsonLoader=new jsonloader;
        //jsonLoader加载json文件结果状态通知
        connect(jsonLoader, &jsonloader::loadSuccess, this, [this](const QString& filePath){
            ui->stateMsg->appendPlainText(QString("json文件加载成功: %1").arg(filePath));
        });
        connect(jsonLoader, &jsonloader::loadFailed, this, [this](const QString& filePath, const QString& errorMsg){
            ui->stateMsg->appendPlainText(QString("json文件加载失败: %1, 错误: %2").arg(filePath).arg(errorMsg));
        });
    }

    if(isCoordinator)
    {
        (serverNetWorker = new coornetworker(localHostName,localHostNum))->moveToThread(trd[NET]=new QThread);
        (coorJsonWorker = new coorjsonworker(localHostName, localHostNum, &serverNetWorker->hostSocketMap, onlineMode,ipRoute))->moveToThread(trd[JS]=new QThread);

        connect(serverNetWorker, &coornetworker::onJsonMsg, coorJsonWorker, &coorjsonworker::onExternalMsg);

        connect(coorJsonWorker, &coorjsonworker::goCreateOfferER, dcManager, &dcmanager::createOfferER);
        connect(coorJsonWorker, &coorjsonworker::goCreateLocalAnswerER, dcManager, &dcmanager::createAnswerER);
        connect(coorJsonWorker, &coorjsonworker::goSetAnswer, dcManager, &dcmanager::setAnswer);
        connect(coorJsonWorker, &coorjsonworker::goSetCandidate, dcManager, &dcmanager::setCandidate);
        connect(coorJsonWorker, &coorjsonworker::sendToNetWorker, serverNetWorker, &coornetworker::sendMsg);

        connect(dcManager, &dcmanager::transferWorkerMsg, coorJsonWorker, &coorjsonworker::onInternalMsg);

        if(!onlineMode)
        {
            connect(jsonLoader, &jsonloader::jsonObjLoaded, coorJsonWorker,
                    std::bind(&coorjsonworker::onExternalMsg, coorJsonWorker, std::placeholders::_1, nullptr));
            connect(coorJsonWorker, &coorjsonworker::offlineFileSaved, this, [this](const QString& msg){
                ui->stateMsg->appendPlainText(msg);
            });
        }
    }
    else
    {
        (clientNetWorker = new peernetworker)->moveToThread(trd[NET]=new QThread);
        (peerJsonWorker=new peerjsonworker(localHostName, localHostNum, dcManager->nameRoute, onlineMode,ipRoute))->moveToThread(trd[JS]=new QThread);

        connect(peerJsonWorker, &peerjsonworker::goCreateOfferER, dcManager, &dcmanager::createOfferER);
        connect(peerJsonWorker, &peerjsonworker::goCreateAnswerER, dcManager, &dcmanager::createAnswerER);
        connect(peerJsonWorker, &peerjsonworker::goSetAnswer, dcManager, &dcmanager::setAnswer);
        connect(peerJsonWorker, &peerjsonworker::goSetCandidate, dcManager, &dcmanager::setCandidate);
        connect(peerJsonWorker, &peerjsonworker::hostNumAssigned, this, [this](int hostNum){
            localHostNum = hostNum;
            ui->stateMsg->appendPlainText(QString("已分配主机号: %1, 虚拟地址: %2.%3")
                                              .arg(hostNum)
                                              .arg(ui->subnetPrefix->text())
                                              .arg(hostNum));
            getTun();
        });

        connect(dcManager, &dcmanager::transferWorkerMsg, peerJsonWorker, &peerjsonworker::onInternalMsg);
        connect(peerJsonWorker, &peerjsonworker::sendToNetWorker, clientNetWorker, &peernetworker::sendMsg);

        connect(clientNetWorker, &peernetworker::readySendHostName, peerJsonWorker, &peerjsonworker::onReadySendHostName);
        connect(clientNetWorker, &peernetworker::onJsonMsg, peerJsonWorker, &peerjsonworker::onExternalMsg);

        if(!onlineMode)
        {
            connect(jsonLoader, &jsonloader::jsonObjLoaded, peerJsonWorker, &peerjsonworker::onExternalMsg);
            connect(peerJsonWorker, &peerjsonworker::offlineFileSaved, this, [this](const QString& msg){
                ui->stateMsg->appendPlainText(msg);
            });
        }
    }

    //原/networker=>jsonworker=>dcmanager=>dcworker
    //jsonloader=>jsonworker=>dcmanager=>dcworker
    //新/dcworker1/2/3=>dcmanager=>jsonworker=打包并投递>dcworker
    //=>对端dcworker=>dcmanager=>jsonworker=>dcmanager=>dcworker
    if(!onlineMode)
    {
        if(isCoordinator)
            connect(dcManager,&dcmanager::onSignalingBackMsg,coorJsonWorker,
                std::bind(&coorjsonworker::onExternalMsg, coorJsonWorker, std::placeholders::_1, nullptr));
        else
            connect(dcManager,&dcmanager::onSignalingBackMsg,peerJsonWorker,&peerjsonworker::onExternalMsg);
    }

    trd[NET]->start();
    trd[JS]->start();
    //注意区分jsonWorker和jsonLoader
    //前者在worker线程运行,后者在主线程运行
    //且前者是无条件构造,作为netWorker和dcManger的中间层
}

void MainWindow::initialTun()
{
    tunLoader = new tunloader;
    if(!tunLoader->load())
    {
        ui->stateMsg->appendPlainText("WinTun初始化失败");
        ui->btnStart->setEnabled(false);
        return;
    }
    tunManager = new tunmanager(tunLoader);
}

void MainWindow::getTun()
{
    QString subnetPrefix = ui->subnetPrefix->text();
    int hostNum = localHostNum;
    QString adapterIP = subnetPrefix + "." + QString::number(hostNum);
    int networkLen = ui->networkLen->value();

    adapter = tunManager->initialAdapter(adapterIP, networkLen,
                                         isCoordinator ? L"QNetLink_Coordinator" : L"QNetLink_Peer");
    if(!adapter)
    {
        ui->stateMsg->appendPlainText("虚拟网卡创建失败（请以管理员身份运行）");
        return;
    }

    session = tunManager->getSession(adapter);
    if(!session)
    {
        ui->stateMsg->appendPlainText("Session获取失败");
        tunManager->shutTun(nullptr, adapter);
        adapter = nullptr;
        return;
    }

    ui->localIP->setText(adapterIP);
    ui->stateMsg->appendPlainText(QString("虚拟网卡已创建: %1/%2").arg(adapterIP).arg(networkLen));

    startTun();
}

void MainWindow::startTun()
{
    if(!tunInWorker)
    {
        tunInWorker = new tuninworker(session, tunLoader, inboundBuffer, mutex);
        tunInWorker->moveToThread(trd[TIN] = new QThread);
        trd[TIN]->start();
    }
    QMetaObject::invokeMethod(tunInWorker, "startInternalSessionFlood", Qt::QueuedConnection);

    if(!tunOutWorker)
    {
        tunOutWorker = new tunoutworker(tunLoader);
        tunOutWorker->moveToThread(trd[TOUT] = new QThread);
        trd[TOUT]->start();
    }

    tunOutWorker->sessionRunning = true;
    QMetaObject::invokeMethod(tunOutWorker, "startExternalSessionFlood", Qt::QueuedConnection,Q_ARG(void*,session),Q_ARG(void*,&dcManager->ipRoute));

    ui->stateMsg->appendPlainText("组网已启动");
}

void MainWindow::onReturnResult(uint64_t timePoint,bool accept)
{
    qLog()<<"[FILE-UI] onReturnResult timePoint="<<timePoint<<" accept="<<accept<<" mission.contains="<<mission.contains(timePoint);
    if(accept && mission.contains(timePoint))
        mission[timePoint].execute();
    //当key不存在时会尝试创建默认request对象，但request没有默认构造函数 => 报错
    mission.remove(timePoint);
}

void MainWindow::cleanUp(bool isShutDown)
{
    closeVideoChatWindow();

    //(新增)关闭所有音频通话弹窗:避免 mainwindow 关闭后弹窗孤立
    for(auto it = audioChatWindows.begin(); it != audioChatWindows.end(); ++it)
    {
        if(it.value())
        {
            it.value()->close();
            it.value()->deleteLater();
        }
    }
    audioChatWindows.clear();

    if(enCoder)
    {
        QMetaObject::invokeMethod(enCoder, "shutdown", Qt::BlockingQueuedConnection);//跨线程阻塞等待确保摄像头和encoder正确释放
        trd[EN]->quit();
        trd[EN]->wait();
        delete(trd[EN]);
        delete enCoder;enCoder=nullptr;//对于btnShut/shutdown都销毁enCoder
    }

    //音频通话资源清理(capture/encoder/player 共用 trd[AU])
    //顺序: 先停掉 capture 的 floodTimer(如果还活着),再 quit trd[AU],再 delete 各组件
    if(audioCap)
    {
        QMetaObject::invokeMethod(audioCap,"shutdown",Qt::BlockingQueuedConnection);
        delete audioCap;audioCap=nullptr;
    }
    if(audioEnCoder)
    {
        QMetaObject::invokeMethod(audioEnCoder,"shutdown",Qt::BlockingQueuedConnection);
        delete audioEnCoder;audioEnCoder=nullptr;
    }
    if(audioPlayer)
    {
        QMetaObject::invokeMethod(audioPlayer,"shutdown",Qt::BlockingQueuedConnection);
        delete audioPlayer;audioPlayer=nullptr;
    }
    if(trd[AU] && trd[AU]->isRunning())
    {
        trd[AU]->quit();
        trd[AU]->wait();
        delete trd[AU];trd[AU]=nullptr;
    }

    if(tunOutWorker)
        tunOutWorker->sessionRunning = false;

    if(tunInWorker)
        if(isShutDown)
            QMetaObject::invokeMethod(tunInWorker, "cleanQOBJ", Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(tunInWorker, "pasueInternalSessionFlood", Qt::QueuedConnection);

    if(!fileSenderContanier.isEmpty())
        for(auto beg=fileSenderContanier.begin();beg!=fileSenderContanier.end();beg++)
        {
            beg.key()->running=false;
            beg.key()->deleteLater();
            beg.value()->quit();//退出事件循环后
            //QThread::finished->Container.remove()+deleteLater()
        }

    if(dcManager)
    {
        for(auto workerGroup : dcManager->ipRoute)
            for(dcworker* worker:workerGroup)
                QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        if(isShutDown)
            QMetaObject::invokeMethod(dcManager, "cleanQOBJ", Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(dcManager, "stopTimer", Qt::QueuedConnection);
    }

    if(isCoordinator)
        QMetaObject::invokeMethod(serverNetWorker,"pauseTcpServer",Qt::QueuedConnection);
    else
        QMetaObject::invokeMethod(clientNetWorker,"pauseTcpClient",Qt::QueuedConnection);

    QTimer* waitTimer = new QTimer(this);
    waitTimer->setInterval(500);

    if(isShutDown)
    {
        ipRoute=nullptr;
        connect(waitTimer, &QTimer::timeout, this, [this, waitTimer](){
            if((fileSenderContanier.isEmpty())&&(!dcManager || dcManager->ipRoute.isEmpty()))
            {
                waitTimer->stop();
                waitTimer->deleteLater();
                for(QThread* thread:trd)
                    if(thread)
                    {
                        thread->quit();
                        thread->wait();
                        delete thread;
                    }
                std::memset(trd,0,4*trdAmount);
                releaseTunResource();
                delete tunManager; tunManager = nullptr;
                delete tunLoader; tunLoader = nullptr;
                if(tunInWorker) { delete tunInWorker; tunInWorker = nullptr; }
                if(tunOutWorker) { delete tunOutWorker; tunOutWorker = nullptr; }
                if(dcManager) { delete dcManager; dcManager = nullptr; }
                if(serverNetWorker) { delete serverNetWorker; serverNetWorker = nullptr; }
                if(clientNetWorker){delete clientNetWorker;clientNetWorker=nullptr;}
                if(deCoder){delete deCoder;deCoder=nullptr;}
                if(jsonLoader){delete(jsonLoader);jsonLoader=nullptr;}
                delete mutex; mutex = nullptr;
                QCoreApplication::quit();
            }
        });
    }
    else
    {
        if(!isCoordinator)
            localHostNum=0;
        connect(waitTimer,&QTimer::timeout,this,[this,waitTimer](){
            if(
                (!dcManager||dcManager->ipRoute.isEmpty())&&
                (fileSenderContanier.isEmpty())&&
                (!tunOutWorker||(!(tunOutWorker->sessionRunning))))
            {
                ui->btnStart->setEnabled(true);
                ui->stateMsg->appendPlainText("已退出组网");
                waitTimer->stop();
                waitTimer->deleteLater();
                releaseTunResource();
                ui->btnStart->setEnabled(true);
            }
        });
    }
    waitTimer->start();
}

void MainWindow::releaseTunResource()
{
    if(session || adapter)
    {
        tunManager->shutTun(session, adapter);
        session = nullptr; adapter = nullptr;
    }
}

QString MainWindow::getCurrentPeerName()
{
    return peerNames.value(currentPeerHostNum, QString());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if(!isClosing) {
        isClosing = true;
        event->ignore();
        setEnabled(false);
        statusBar()->showMessage("正在关闭...");
        cleanUp(true);
    } else {
        event->accept();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
