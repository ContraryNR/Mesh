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
        if(isCoordinator)
        {
            currentNetConf = getCoordinateNetConfigFromUI();
            if(!currentNetConf.isValid())
            {
                ui->stateMsg->appendPlainText("配置信息不合法");
                return;
            }
            QMetaObject::invokeMethod(netWorker, "startTcpServer", Qt::QueuedConnection,
                                      Q_ARG(const QString&, currentNetConf.ip),
                                      Q_ARG(int, currentNetConf.port));
            ui->stateMsg->appendPlainText(QString("协调者监听在 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
            ui->btnStart->setEnabled(false);
            getTun();
        }
        else
        {
            currentNetConf = getPeerNetConfigFromUI();
            if(!currentNetConf.isValid())
            {
                ui->stateMsg->appendPlainText("配置信息不合法");
                return;
            }
            QMetaObject::invokeMethod(netWorker, "startTcpClient", Qt::QueuedConnection,
                                      Q_ARG(const QString&, currentNetConf.ip),
                                      Q_ARG(int, currentNetConf.port));
            ui->stateMsg->appendPlainText(QString("正在连接协调者 %1:%2").arg(currentNetConf.ip).arg(currentNetConf.port));
            ui->btnStart->setEnabled(false);
        }
        ui->btnSend->setEnabled(true);
        ui->btnBroadcast->setEnabled(true);
        ui->btnShut->setEnabled(true);
        if(dcManager)
            QMetaObject::invokeMethod(dcManager,"startTimer",Qt::QueuedConnection);
    });

    connect(ui->btnShut, &QPushButton::clicked, this, [this](){
        ui->btnSend->setEnabled(false);
        ui->btnBroadcast->setEnabled(false);
        ui->btnShut->setEnabled(false);
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

    startTunWorker();
}

void MainWindow::startTunWorker()
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
        connect(tunOutWorker, &tunoutworker::sendExternelSpeed, this, [this](int speed){
            if(speed / 1024 <= 1)
                ui->externalSpeed->setText(QString("出站: %1 B/s").arg(speed));
            else if((speed /= 1024) / 1024 <= 1)
                ui->externalSpeed->setText(QString("出站: %1 KB/s").arg(speed));
            else
                ui->externalSpeed->setText(QString("出站: %1 MB/s").arg(speed /= 1024));
        });
        tunOutWorker->moveToThread(trd[OUT] = new QThread);
        trd[OUT]->start();
    }

    tunOutWorker->sessionRunning = true;
    QMetaObject::invokeMethod(tunOutWorker, "startExternalSessionFlood", Qt::QueuedConnection,Q_ARG(void*,session),Q_ARG(void*,&dcManager->ipRoute));

    ui->btnShut->setEnabled(true);
    ui->btnSend->setEnabled(true);
    ui->stateMsg->appendPlainText("组网已启动");
}

netConfig MainWindow::getCoordinateNetConfigFromUI()
{
    return netConfig(ui->listenIP->currentText(), ui->listenPort->value());
}

netConfig MainWindow::getPeerNetConfigFromUI()
{
    return netConfig(ui->coordIP->text(), ui->coordPort->value());
}

void MainWindow::goSendMsg()
{
}

void MainWindow::goSendUnicastMsg()
{
    QString msg = ui->sendingMsg->text();
    if(msg.isEmpty()) return;

    if(currentPeerHostNum == 0 || ui->chatTabWidget->count() == 0) {
        ui->sendingMsg->clear();
        return;
    }

    QString peerName = peerNames.value(currentPeerHostNum, QString::number(currentPeerHostNum));

    QMetaObject::invokeMethod(dcManager, "sendToPeer", Qt::QueuedConnection,
                              Q_ARG(int, currentPeerHostNum),
                              Q_ARG(const QString&, msg));

    QString tabLabel = QString("%1 (%2)").arg(peerName).arg(currentPeerHostNum);
    for(int i = 0; i < ui->chatTabWidget->count(); i++) {
        if(ui->chatTabWidget->tabText(i) == tabLabel) {
            QPlainTextEdit* chatWidget = qobject_cast<QPlainTextEdit*>(ui->chatTabWidget->widget(i));
            if(chatWidget) {
                chatWidget->appendPlainText("本机: " + msg);
            }
            break;
        }
    }
    ui->sendingMsg->clear();
}

void MainWindow::goSendBroadcastMsg()
{
    QString msg = ui->sendingMsg->text();
    if(msg.isEmpty()) return;

    if(ui->chatTabWidget->count() == 0) {
        ui->sendingMsg->clear();
        return;
    }

    QMetaObject::invokeMethod(dcManager, "broadcastMsg", Qt::QueuedConnection,
                              Q_ARG(const QString&, msg));

    for(int i = 0; i < ui->chatTabWidget->count(); i++) {
        QPlainTextEdit* chatWidget = qobject_cast<QPlainTextEdit*>(ui->chatTabWidget->widget(i));
        if(chatWidget) {
            chatWidget->appendPlainText("本机(广播): " + msg);
        }
    }
    ui->sendingMsg->clear();
}

void MainWindow::onPeerAdded(int peerHostNum, const QString& peerHostName)
{
    peerNames.insert(peerHostNum, peerHostName);

    QString virtualIP = ui->subnetPrefix->text() + "." + QString::number(peerHostNum);

    int row = ui->peerTable->rowCount();
    ui->peerTable->insertRow(row);
    ui->peerTable->setItem(row, 0, new QTableWidgetItem(QString::number(peerHostNum)));
    ui->peerTable->setItem(row, 1, new QTableWidgetItem(peerHostName));
    ui->peerTable->setItem(row, 2, new QTableWidgetItem(virtualIP));
    ui->peerTable->setItem(row, 3, new QTableWidgetItem("在线"));

    QPlainTextEdit* chatWidget = new QPlainTextEdit();
    chatWidget->setReadOnly(true);
    QString tabLabel = QString("%1 (%2)").arg(peerHostName).arg(peerHostNum);
    ui->chatTabWidget->addTab(chatWidget, tabLabel);
}

void MainWindow::onPeerRemoved(int peerHostNum, const QString& /*peerHostName*/)
{
    peerNames.remove(peerHostNum);

    for(int i = 0; i < ui->peerTable->rowCount(); i++) {
        if(ui->peerTable->item(i, 0)->text().toInt() == peerHostNum) {
            ui->peerTable->removeRow(i);
            break;
        }
    }

    QString tabLabelToRemove;
    for(int i = 0; i < ui->chatTabWidget->count(); i++) {
        QString tabLabel = ui->chatTabWidget->tabText(i);
        QRegularExpression rx(R"(\((\d+)\))");
        QRegularExpressionMatch match = rx.match(tabLabel);
        if(match.hasMatch() && match.captured(1).toInt() == peerHostNum) {
            QWidget* tabWidget = ui->chatTabWidget->widget(i);
            ui->chatTabWidget->removeTab(i);
            delete tabWidget;
            break;
        }
    }

    if(currentPeerHostNum == peerHostNum) {
        currentPeerHostNum = 0;
    }
}

void MainWindow::onPeerMsgReceived(int peerHostNum, const QString& peerName, const QString& msg)
{
    QString tabLabel = QString("%1 (%2)").arg(peerName).arg(peerHostNum);
    for(int i = 0; i < ui->chatTabWidget->count(); i++) {
        if(ui->chatTabWidget->tabText(i) == tabLabel) {
            QPlainTextEdit* chatWidget = qobject_cast<QPlainTextEdit*>(ui->chatTabWidget->widget(i));
            if(chatWidget) {
                chatWidget->appendPlainText(peerName + ": " + msg);
            }
            break;
        }
    }
}

void MainWindow::onPeerTableClicked(QTableWidgetItem* item)
{
    if(item) {
        int row = item->row();
        currentPeerHostNum = ui->peerTable->item(row, 0)->text().toInt();
        QString peerName = ui->peerTable->item(row, 1)->text();

        QString tabLabel = QString("%1 (%2)").arg(peerName).arg(currentPeerHostNum);
        for(int i = 0; i < ui->chatTabWidget->count(); i++) {
            if(ui->chatTabWidget->tabText(i) == tabLabel) {
                ui->chatTabWidget->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MainWindow::onChatTabChanged(int index)
{
    if(index >= 0 && index < ui->chatTabWidget->count()) {
        QString tabLabel = ui->chatTabWidget->tabText(index);
        QRegularExpression rx(R"(\((\d+)\))");
        QRegularExpressionMatch match = rx.match(tabLabel);
        if(match.hasMatch()) {
            currentPeerHostNum = match.captured(1).toInt();
        }
    }
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
        ui->stateMsg->appendPlainText("已退出组网");
    }
}

void MainWindow::releaseTunResource()
{
    if(session || adapter)
    {tunManager->shutTun(session, adapter);
        session = nullptr; adapter = nullptr;}
}

void MainWindow::initialUI()
{
    bool ok;
    QString name = QInputDialog::getText(this, "主机标识名设置",
                                         "请输入您在此组网中的唯一名称:", QLineEdit::Normal, "", &ok);
    if(!ok || name.trimmed().isEmpty())
        name = QString("Host_%1").arg(QRandomGenerator::global()->bounded(1000, 9999));
    localHostName = name.trimmed();
    ui->hostNameDisplay->setText(localHostName);

    isCoordinator = requestDialog("运行模式选择", "请选择运行模式", "Coordinator", "Peer");
    ui->modeDisplay->setText(isCoordinator ? "Coordinator" : "Peer");
    ui->coordinatorGroup->setVisible(isCoordinator);
    ui->peerGroup->setVisible(!isCoordinator);

    if(isCoordinator)
    {
        for(const QHostAddress& addr : QNetworkInterface::allAddresses())
            if(addr.protocol() == QAbstractSocket::IPv4Protocol)
                ui->listenIP->addItem(addr.toString());
        int index = ui->listenIP->findText("127.0.0.1");
        ui->listenIP->setCurrentIndex(index >= 0 ? index : 0);
    }

    for(QPushButton* button : findChildren<QPushButton*>())
        button->setEnabled(false);
    ui->btnStart->setEnabled(true);
    ui->cleanState->setEnabled(true);
    ui->cleanMessage->setEnabled(true);

    connect(ui->cleanMessage, &QPushButton::clicked, this, [this](){
        for(int i = 0; i < ui->chatTabWidget->count(); i++) {
            QPlainTextEdit* chatWidget = qobject_cast<QPlainTextEdit*>(ui->chatTabWidget->widget(i));
            if(chatWidget) {
                chatWidget->clear();
            }
        }
    });
    connect(ui->cleanState, &QPushButton::clicked, ui->stateMsg, &QPlainTextEdit::clear);
    connect(ui->btnSend, &QPushButton::clicked, this, &MainWindow::goSendUnicastMsg);
    connect(ui->btnBroadcast, &QPushButton::clicked, this, &MainWindow::goSendBroadcastMsg);
    connect(ui->peerTable, &QTableWidget::itemClicked, this, &MainWindow::onPeerTableClicked);
    connect(ui->chatTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onChatTabChanged);
    connect(ui->sendingMsg, &QLineEdit::returnPressed, this, [this](){
        if(ui->btnSend->isEnabled())
            goSendUnicastMsg();
    });
}

bool MainWindow::requestDialog(const QString& title, const QString& text, const QString& btnText1, const QString& btnText0)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    QPushButton* button1 = msgBox.addButton(btnText1, QMessageBox::ActionRole);
    QPushButton* button0 = msgBox.addButton(btnText0, QMessageBox::ActionRole);
    msgBox.setDefaultButton(button1);
    msgBox.exec();
    return (QPushButton*)(msgBox.clickedButton()) == button1 ? 1 : 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

