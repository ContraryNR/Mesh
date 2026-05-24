#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), mutex(new QMutex)
{
    ui->setupUi(this);
    initialUI();
    initialSignaling();
    initialTun();

    connect(ui->btnStart, &QPushButton::clicked, this, [this](){
        ui->btnStart->setEnabled(false);
        if(isCoordinator)
        {
            currentNetConf = getCoordinateNetConfigFromUI();
            if(!currentNetConf.isValid())
            {
                ui->stateMsg->appendPlainText("配置信息不合法");
                ui->btnStart->setEnabled(true);
                return;
            }
            QMetaObject::invokeMethod(netWorker, "startTcpServer", Qt::QueuedConnection,
                                      Q_ARG(const QString&, currentNetConf.ip),
                                      Q_ARG(int, currentNetConf.port));
            ui->stateMsg->appendPlainText(QString("协调者监听在 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
            getTun();
        }
        else
        {
            currentNetConf = getPeerNetConfigFromUI();
            if(!currentNetConf.isValid())
            {
                ui->stateMsg->appendPlainText("配置信息不合法");
                ui->btnStart->setEnabled(true);
                return;
            }
            QMetaObject::invokeMethod(netWorker, "startTcpClient", Qt::QueuedConnection,
                                      Q_ARG(const QString&, currentNetConf.ip),
                                      Q_ARG(int, currentNetConf.port));
            ui->stateMsg->appendPlainText(QString("正在连接协调者 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
        }
        ui->btnShut->setEnabled(true);
        if(dcManager)
            QMetaObject::invokeMethod(dcManager,"startTimer",Qt::QueuedConnection);
    });

    connect(ui->btnShut, &QPushButton::clicked, this, [this](){
        ui->btnSend->setEnabled(false);
        ui->btnBroadcast->setEnabled(false);
        ui->btnAttach->setEnabled(false);
        ui->btnShut->setEnabled(false);
        currentRow=-1;//需要重置为-1,否则如果停止组网前和再次组网后都只有一台主机则row和currentRow一直相等=>不触发ui更新
        cleanUp(false);
    });
}

void MainWindow::initialSignaling()
{
    (dcManager = new dcmanager(inboundBuffer, mutex))->moveToThread(trd[DC] = new QThread);
    connect(dcManager, &dcmanager::sendInboundSpeed, this, [this](int speed){
        if(speed / 1024 <= 1)
            ui->internalSpeed->setText(QString("入站: %1 B/s").arg(speed));
        else if((speed /= 1024) / 1024 <= 1)
            ui->internalSpeed->setText(QString("入站: %1 KB/s").arg(speed));
        else
            ui->internalSpeed->setText(QString("入站: %1 MB/s").arg(speed /= 1024));
    });
    connect(dcManager, &dcmanager::sendOutboundSpeed, this, [this](int speed){
        if(speed / 1024 <= 1)
            ui->externalSpeed->setText(QString("出站: %1 B/s").arg(speed));
        else if((speed /= 1024) / 1024 <= 1)
            ui->externalSpeed->setText(QString("出站: %1 KB/s").arg(speed));
        else
            ui->externalSpeed->setText(QString("出站: %1 MB/s").arg(speed /= 1024));
    });
    trd[DC]->start();

    if(isCoordinator)
    {
        netWorker = new servernetworker(localHostName);
        connect(dcManager, &dcmanager::sendMsg, (servernetworker*)netWorker, &servernetworker::transferWorkerMsg);
        connect((servernetworker*)netWorker, &servernetworker::goCreateLocalAnswerER, dcManager, &dcmanager::createAnswerER);
        connect((servernetworker*)netWorker, &servernetworker::goSetCandidate, dcManager, &dcmanager::setCandidate);
    }
    else
    {
        netWorker = new clientnetworker(localHostName);
        connect(dcManager, &dcmanager::sendMsg, (clientnetworker*)netWorker, &clientnetworker::transferWorkerMsg);
        connect((clientnetworker*)netWorker, &clientnetworker::goCreateOfferER, dcManager, &dcmanager::createOfferER);
        connect((clientnetworker*)netWorker, &clientnetworker::goCreateAnswerER, dcManager, &dcmanager::createAnswerER);
        connect((clientnetworker*)netWorker, &clientnetworker::goSetAnswer, dcManager, &dcmanager::setAnswer);
        connect((clientnetworker*)netWorker, &clientnetworker::goSetCandidate, dcManager, &dcmanager::setCandidate);

        connect((clientnetworker*)netWorker, &clientnetworker::hostNumAssigned, this, [this](int hostNum){
            ui->stateMsg->appendPlainText(QString("已分配主机号: %1, 虚拟地址: %2.%3")
                                              .arg(hostNum)
                                              .arg(ui->subnetPrefix->text())
                                              .arg(hostNum));
            getTun();
        });
    }

    connect(dcManager, &dcmanager::peerAdded, this, &MainWindow::onPeerAdded);
    connect(dcManager, &dcmanager::peerRemoved, this, &MainWindow::onPeerRemoved);
    connect(dcManager, &dcmanager::receiveStringMsg, this, &MainWindow::onPeerMsgReceived);

    netWorker->moveToThread(trd[NET] = new QThread);
    trd[NET]->start();
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
    int hostNum = netWorker->localHostNum;
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
        tunInWorker->moveToThread(trd[IN] = new QThread);
        trd[IN]->start();
    }
    QMetaObject::invokeMethod(tunInWorker, "startInternalSessionFlood", Qt::QueuedConnection);

    if(!tunOutWorker)
    {
        tunOutWorker = new tunoutworker(tunLoader);
        tunOutWorker->moveToThread(trd[OUT] = new QThread);
        trd[OUT]->start();
    }

    tunOutWorker->sessionRunning = true;
    QMetaObject::invokeMethod(tunOutWorker, "startExternalSessionFlood", Qt::QueuedConnection,Q_ARG(void*,session),Q_ARG(void*,&dcManager->ipRoute));

    ui->stateMsg->appendPlainText("组网已启动");
}

void MainWindow::cleanUp(bool isShutDown)
{
    if(tunOutWorker)
        tunOutWorker->sessionRunning = false;

    if(tunInWorker)
    {
        if(isShutDown)
            QMetaObject::invokeMethod(tunInWorker, "cleanQOBJ", Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(tunInWorker, "pasueInternalSessionFlood", Qt::QueuedConnection);
    }



    if(dcManager)
    {
        for(auto worker : dcManager->ipRoute)
            QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        if(isShutDown)
            QMetaObject::invokeMethod(dcManager, "cleanQOBJ", Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(dcManager, "stopTimer", Qt::QueuedConnection);
    }

    if(netWorker)
    {
        if(isCoordinator)
            QMetaObject::invokeMethod((servernetworker*)netWorker,"pauseTcpServer",Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod((clientnetworker*)netWorker,"pauseTcpClient",Qt::QueuedConnection);
    }//停止组网时一并释放tcp连接(1)避免不必要的资源占用(2)方便下次直接使用ui最新数据启动tcpNetWorker

    QTimer* waitTimer = new QTimer(this);
    waitTimer->setInterval(500);

    if(isShutDown)
    {
        connect(waitTimer, &QTimer::timeout, this, [this, waitTimer](){
            bool dcDone = !dcManager || dcManager->ipRoute.isEmpty();
            if(dcDone)
            {
                waitTimer->stop();
                waitTimer->deleteLater();
                for(int i = 0; i < 4; i++)
                    if(trd[i])
                    {
                        trd[i]->quit();
                        trd[i]->wait();
                        delete trd[i];
                        trd[i] = nullptr;
                    }
                releaseTunResource();
                delete tunManager; tunManager = nullptr;
                delete tunLoader; tunLoader = nullptr;
                if(tunInWorker) { delete tunInWorker; tunInWorker = nullptr; }
                if(tunOutWorker) { delete tunOutWorker; tunOutWorker = nullptr; }
                if(dcManager) { delete dcManager; dcManager = nullptr; }
                if(netWorker) { delete netWorker; netWorker = nullptr; }
                //~networker => ~QObject =>~tcpServer =~children> ~tcpSocket
                //其中server和socket的析构都不应涉及currentThreadData().tls.eventDispathcher故不必担心跨线程安全问题
                delete mutex; mutex = nullptr;
                QCoreApplication::quit();
            }
        });
        waitTimer->start();
    }
    else
    {
        if(!isCoordinator&&netWorker)
            netWorker->localHostNum=0;
        if(tunOutWorker)
        {
            connect(waitTimer,&QTimer::timeout,this,[this,waitTimer](){
                if(!(tunOutWorker->floodFinish))
                    return;
                waitTimer->stop();
                waitTimer->deleteLater();
                releaseTunResource();
                ui->btnStart->setEnabled(true);
            });
            waitTimer->start();
        }
        else//针对Peer尚未分配到主机号的情况
            ui->btnStart->setEnabled(true);
        ui->stateMsg->appendPlainText("已退出组网");
    }
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

