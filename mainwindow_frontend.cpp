#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QInputDialog>
#include <QFileInfo>
#include <QProgressBar>

//(0)dcManger->MainWindow同步更新
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
    ui->peerTable->setItem(row, 4, new QTableWidgetItem("1"));
    if(!peerChatHistory.contains(peerHostNum))
        peerChatHistory.insert(peerHostNum, QStringList());
    ui->btnBroadcast->setEnabled(true);
}
void MainWindow::onPeerRemoved(int peerHostNum)
{
    peerNames.remove(peerHostNum);
    peerChatHistory.remove(peerHostNum);
    for(int i = 0; i < ui->peerTable->rowCount(); i++)
    {
        QTableWidgetItem* item = ui->peerTable->item(i, 0);
        if(item && item->text().toInt() == peerHostNum) {
            ui->peerTable->removeRow(i);
            break;
        }
    }
    if(currentPeerHostNum == peerHostNum)
    {
        currentPeerHostNum = 0;
        ui->chatTitle->setText("未选择聊天对象");
        ui->chatListWidget->clear();
        ui->btnAttach->setEnabled(false);
        ui->btnSend->setEnabled(false);
        ui->btnVoiceChat->setEnabled(false);
        ui->btnVideoChat->setEnabled(false);
    }
    if(peerNames.size()==0)
        ui->btnBroadcast->setEnabled(false);
}
void MainWindow::onPeerConnectionAmountChanged(int peerHostNum,int currentConnectAmount)
{
    for(int i = 0; i < ui->peerTable->rowCount(); i++)
    {
        QTableWidgetItem* item = ui->peerTable->item(i, 0);
        if(item && item->text().toInt() == peerHostNum)
        {
            QTableWidgetItem* connItem = ui->peerTable->item(i, 4);
            if(connItem)
                connItem->setText(QString::number(currentConnectAmount));
            break;
        }
    }
    if(currentPeerHostNum == peerHostNum)
        updateCallButtonState();
}
void MainWindow::onWorkerPulse(int ibs, int obs, int pr, const QList<fileDownLoadState> &fileState)
{
    if(ibs / 1024 <= 1)
        ui->internalSpeed->setText(QString("入站: %1 B/s").arg(ibs));
    else if((ibs /= 1024) / 1024 <= 1)
        ui->internalSpeed->setText(QString("入站: %1 KB/s").arg(ibs));
    else
        ui->internalSpeed->setText(QString("入站: %1 MB/s").arg(ibs /= 1024));

    if(obs / 1024 <= 1)
        ui->externalSpeed->setText(QString("出站: %1 B/s").arg(obs));
    else if((obs /= 1024) / 1024 <= 1)
        ui->externalSpeed->setText(QString("出站: %1 KB/s").arg(obs));
    else
        ui->externalSpeed->setText(QString("出站: %1 MB/s").arg(obs /= 1024));

    if(pr > 0)
        ui->pendingStackSize->setText(QString("待发积压: %1").arg(pr));
    else
        ui->pendingStackSize->setText("待发积压: --");

    if(!fileState.isEmpty())
        onFileDownLoadState(fileState);
}
//(1.1)收信
void MainWindow::addChatBubble(QListWidget* listWidget, const QString& text, bool isSelf, bool isBroadcast)
{
    QWidget* bubbleContainer = new QWidget();
    QHBoxLayout* containerLayout = new QHBoxLayout(bubbleContainer);
    containerLayout->setContentsMargins(isSelf ? 60 : 8, 4, isSelf ? 8 : 60, 4);
    containerLayout->setSpacing(0);
    QLabel* bubbleLabel = new QLabel(isBroadcast ? QString("[广播] %1").arg(text) : text);
    bubbleLabel->setWordWrap(true);
    bubbleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubbleLabel->setStyleSheet(isSelf
                                   ? "background-color: #DCF8C6; border-radius: 10px; padding: 8px 12px; color: #333;"
                                   : "background-color: #FFFFFF; border-radius: 10px; padding: 8px 12px; color: #333; border: 1px solid #ddd;");
    if(isSelf)
        containerLayout->addStretch();
    containerLayout->addWidget(bubbleLabel);
    if(!isSelf)
        containerLayout->addStretch();
    QListWidgetItem* item = new QListWidgetItem(listWidget);
    item->setSizeHint(QSize(bubbleContainer->sizeHint().width(), bubbleContainer->sizeHint().height() + 8));
    listWidget->setItemWidget(item, bubbleContainer);
    listWidget->scrollToBottom();
}
void MainWindow::reloadChatHistory()
{
    ui->chatListWidget->clear();
    QStringList history = peerChatHistory.value(currentPeerHostNum, QStringList());
    for(const QString& item : history)
    {
        QStringList parts = item.split("|");
        if(parts.size() >= 2)
        {
            bool isSelf = parts[0] == "self";
            bool isBroadcast = parts.size() >= 3 && parts[1] == "broadcast";
            QString msg = isBroadcast ? parts[2] : parts[1];
            addChatBubble(ui->chatListWidget, msg, isSelf, isBroadcast);
        }
    }
}
void MainWindow::onPeerTableClicked(QTableWidgetItem* item)
{
    if(item)
    {
        int row = item->row();
        if(currentRow!=row)
            currentRow=row;
        else
            return;
        if(row < 0 || row >= ui->peerTable->rowCount())
            return;
        QTableWidgetItem* hostNumItem = ui->peerTable->item(row, 0);
        QTableWidgetItem* peerNameItem = ui->peerTable->item(row, 1);
        if(!hostNumItem || !peerNameItem)
            return;
        currentPeerHostNum = hostNumItem->text().toInt();
        QString peerName = peerNameItem->text();
        ui->chatTitle->setText(QString("与 %1 (%2) 的对话").arg(peerName).arg(currentPeerHostNum));
        ui->btnAttach->setEnabled(true);
        ui->btnSend->setEnabled(true);
        reloadChatHistory();
        updateFileTransferTable();
        updateCallButtonState();
    }
}
void MainWindow::onPeerMsgReceived(int peerHostNum,const QString& msg)
{
    if(!peerNames.contains(peerHostNum))
        return;
    peerChatHistory[peerHostNum].append(QString("peer|%1").arg(msg));
    if(currentPeerHostNum == peerHostNum)
        addChatBubble(ui->chatListWidget, msg, false, false);
}
//(1.2)收文件
void MainWindow::updateFileReceiveTable()
{
    ui->fileReceiveTable->setRowCount(0);
    for (auto it = fileReceiveHash.begin(); it != fileReceiveHash.end(); ++it)
    {
        int peerNum = it.key();
        const QVector<FileReceiveInfo>& vec = it.value();
        for (const FileReceiveInfo& info : vec)
        {
            int row = ui->fileReceiveTable->rowCount();
            ui->fileReceiveTable->insertRow(row);
            ui->fileReceiveTable->setItem(row, 0, new QTableWidgetItem(info.fileName));
            ui->fileReceiveTable->setItem(row, 1, new QTableWidgetItem(peerNames.value(peerNum, QString::number(peerNum))));
            QProgressBar* progressBar = new QProgressBar();
            progressBar->setRange(0, 100);
            progressBar->setValue(static_cast<int>(info.progress));
            progressBar->setFormat(QString("%1%").arg(static_cast<int>(info.progress)));
            ui->fileReceiveTable->setCellWidget(row, 2, progressBar);
        }
    }
}
void MainWindow::onFileDownLoadState(const QList<fileDownLoadState> & stateList)
{
    for (const fileDownLoadState& state : stateList)
    {
        int peerNum = state.peerHostNum;
        if (!fileReceiveHash.contains(peerNum))
            fileReceiveHash.insert(peerNum, QVector<FileReceiveInfo>());
        QVector<FileReceiveInfo>& vec = fileReceiveHash[peerNum];
        bool found = false;
        for (auto& info : vec)
        {
            if (info.fileName == state.filename)
            {
                info.progress = state.progress;
                found = true;
                break;
            }
        }
        if (!found)
            vec.append(FileReceiveInfo(state.filename, state.progress));
    }
    updateFileReceiveTable();
}

void MainWindow::onFileDownLoadFinish(const QString & fileName, int peerHostNum)
{
    ui->stateMsg->appendPlainText(QString("来自'%1'的文件'%2'接收完毕").arg(peerNames[peerHostNum]).arg(fileName));
    if (fileReceiveHash.contains(peerHostNum))
    {
        QVector<FileReceiveInfo>& vec = fileReceiveHash[peerHostNum];
        for (auto& info : vec)
        {
            if (info.fileName == fileName)
            {
                info.progress = 100;
                break;
            }
        }
    }
    updateFileReceiveTable();
}
//(2.1)发信
void MainWindow::goSendUnicastMsg()
{
    QString msg = ui->sendingMsg->text();
    if(msg.isEmpty()) return;
    msg.push_front(QTime::currentTime().toString("HH:mm")+QString("\n"));
    if(currentPeerHostNum == 0)
    {
        ui->sendingMsg->clear();
        return;
    }
    QMetaObject::invokeMethod(getDcWorker((void*)ipRoute,currentPeerHostNum,0), "sendStringMsg", Qt::QueuedConnection,Q_ARG(const QString&, msg));
    peerChatHistory[currentPeerHostNum].append(QString("self|%1").arg(msg));
    addChatBubble(ui->chatListWidget, msg, true, false);
    ui->sendingMsg->clear();
}
void MainWindow::goSendBroadcastMsg()
{
    QString msg = ui->sendingMsg->text();
    if(msg.isEmpty()) return;
    msg.push_front(QTime::currentTime().toString("HH:mm")+QString("\n"));
    if(peerNames.isEmpty())
    {
        ui->sendingMsg->clear();
        return;
    }
    for(dcworker* worker:getBroundCastWorkers((void*)ipRoute))
    {
        worker->newEventNow=true;
        QMetaObject::invokeMethod(worker, "sendStringMsg", Qt::QueuedConnection,Q_ARG(const QString&, msg));
    }
    QString historyItem = QString("self|broadcast|%1").arg(msg);
    for(int hostNum : peerNames.keys()) {
        peerChatHistory[hostNum].append(historyItem);
    }
    if(currentPeerHostNum != 0)
        addChatBubble(ui->chatListWidget, msg, true, true);
    ui->sendingMsg->clear();
}
//(2.2)发文件
QString MainWindow::formatFileSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    if (bytes >= GB)
        return QString("%1 GB").arg(QString::number(bytes / (double)GB, 'f', 2));
    else if (bytes >= MB)
        return QString("%1 MB").arg(QString::number(bytes / (double)MB, 'f', 2));
    else if (bytes >= KB)
        return QString("%1 KB").arg(QString::number(bytes / (double)KB, 'f', 2));
    else
        return QString("%1 B").arg(bytes);
}
void MainWindow::updateFileTransferTable()
{
    ui->fileTransferTable->setRowCount(0);
    for (const FileTransferInfo& info : fileTransferHash.value(currentPeerHostNum))
    {
        int row = ui->fileTransferTable->rowCount();
        ui->fileTransferTable->insertRow(row);
        ui->fileTransferTable->setItem(row, 0, new QTableWidgetItem(info.fileName));
        ui->fileTransferTable->setItem(row, 1, new QTableWidgetItem(formatFileSize(info.fileSize)));
        ui->fileTransferTable->setItem(row, 2, new QTableWidgetItem(info.chunkFinished ? "投递完毕" :(info.terminateException?"异常中断":"切块中")));
        ui->fileTransferTable->setItem(row, 3, new QTableWidgetItem(info.createTime.toString("yyyy-MM-dd HH:mm:ss")));
    }
}
void MainWindow::onAttachFile()
{
    if(!currentPeerHostNum)return;
    QString filePath = QFileDialog::getOpenFileName(this, "选择文件", "", "所有文件 (*.*)");
    if(!filePath.isEmpty())
    {
        ui->stateMsg->appendPlainText(QString("选中文件: %1 => 待对方确认接收").arg(filePath));
        QFileInfo fileInfo(filePath);
        QString fileName=fileInfo.fileName();
        dcworker* worker=getDcWorker((void*)ipRoute,currentPeerHostNum,1);

        uint64_t requestTime=getRunningTime();
        QMetaObject::invokeMethod(worker,"sendFileMsg",Qt::QueuedConnection,
        Q_ARG(const QByteArray&,createRequest(0,QString("对方%1请求发送文件'%2'给你").arg(localHostName).arg(fileName),requestTime)),Q_ARG(bool,false));

        mission.emplace(requestTime,this,
                        [this,filePath,fileInfo,fileName,worker]()
            {
                QPointer<dcworker> safeDcWorker(worker);
                if(!safeDcWorker)
                    return;

                FileTransferInfo transferInfo(fileName,fileInfo.size(),QDateTime::currentDateTime(),false);
                fileTransferHash[currentPeerHostNum].append(transferInfo);

                filesender* fileSender;QThread* trd;
                fileSenderContanier.insert(fileSender=new filesender(filePath),trd=new QThread);
                fileSender->moveToThread(trd);trd->start();fileSender->running=true;

                QMetaObject::invokeMethod(fileSender,"sendFile",Qt::QueuedConnection,Q_ARG(void*,(void*)worker));

                connect(fileSender,&filesender::fileSendStop,this,[this, trd, fileName](bool isFinish){
                    trd->quit();
                    for (FileTransferInfo& info : fileTransferHash[currentPeerHostNum])
                        if (info.fileName == fileName)
                        {
                            if(isFinish)
                                info.chunkFinished = isFinish;
                            else
                                info.terminateException=true;
                            break;
                        }
                    updateFileTransferTable();
                });
                connect(trd,&QThread::finished,this,[this,fileSender,trd](){
                    fileSenderContanier.remove(fileSender);
                    trd->deleteLater();
                });
            });
    }
}
//(3)设置dcWorker
void MainWindow::onSettingsClicked()
{
    if(!dcManager)
        return;

    SettingsDialog dialog(dcManager->busySize, dcManager->freeSize,
                          dcManager->nameRoute, dcManager->ipRoute,
                          ui->subnetPrefix->text(), this);

    connect(&dialog, &SettingsDialog::applyConnectionCount, this, [this](int targetAmount, const QList<int>& peerHostNums){
        for(int peerHostNum : peerHostNums)
        {
            if(!ipRoute || !ipRoute->contains(peerHostNum))
                continue;

            int currentAmount = ipRoute->value(peerHostNum).size();
            if(currentAmount == targetAmount)
                continue;

            if(targetAmount > currentAmount)
                QMetaObject::invokeMethod(dcManager, "getExtraConnection", Qt::QueuedConnection,
                                          Q_ARG(int, targetAmount), Q_ARG(int, peerHostNum));
            else
                QMetaObject::invokeMethod(dcManager, "releaseExtraConnection", Qt::QueuedConnection,
                                          Q_ARG(int, targetAmount), Q_ARG(int, peerHostNum));
        }
    });

    if(dialog.exec() == QDialog::Accepted)
    {
        int busySize = dialog.getBusySize();
        int freeSize = dialog.getFreeSize();

        QMetaObject::invokeMethod(dcManager, "updateAllDcWorkerSettings", Qt::QueuedConnection,
                                  Q_ARG(int, busySize), Q_ARG(int, freeSize));

        ui->stateMsg->appendPlainText(QString("参数已更新 - busySize: %1, freeSize: %2")
                                          .arg(busySize).arg(freeSize));
    }
}

//(4)视频通话
void MainWindow::onDecodedFrame(const QImage & frameImg, int peerHostNum)
{
    if (!videoChatSessions.contains(peerHostNum))
    {
        confirmStartVideoChat(peerHostNum);
        if(!enCoder)
        {
            (enCoder=new videoencoder)->moveToThread(trd[EN]=new QThread);
            trd[EN]->start();
            QMetaObject::invokeMethod(enCoder,"startFloodTimer",Qt::QueuedConnection);

            connect(enCoder, &videoencoder::sendEncodedFrame, this, [this](const QImage& localFrame, const QByteArray& encodedData){
                for(auto it = videoChatSessions.begin(); it != videoChatSessions.end(); ++it)
                {
                    VideoChatWindow* videoWindow = it.value().first;
                    if(videoWindow)
                        videoWindow->showLocalVideo(localFrame);
                }
                if(ipRoute)
                    for(dcworker* worker:getCallingPeerWorkers(ipRoute,videoChatSessions.keys()))
                        QMetaObject::invokeMethod(worker, "sendVideoMsg", Qt::QueuedConnection,Q_ARG(const QByteArray&, encodedData));
            });

            connect(trd[EN],&QThread::finished,this,[this](){
                trd[EN]->deleteLater();
                if(enCoder)
                {
                    delete(enCoder);
                    enCoder=nullptr;
                }
                trd[EN]=nullptr;
            });
        }
    }

    VideoChatWindow* videoWindow = videoChatSessions.value(peerHostNum).first;
    if (!videoWindow)
        return;

    QString peerName = peerNames.value(peerHostNum, "未知");
    videoWindow->showRemoteVideo(frameImg, peerHostNum);
    videoWindow->setPeerName(peerName);
}

void MainWindow::updateVideoChatButtonState()
{
    bool hasActiveSession = !videoChatSessions.isEmpty();
    ui->btnVideoChat->setEnabled(!hasActiveSession);//目前暂时设计为"仅允许同时进行一通话"
    ui->btnVideoChat->setText(hasActiveSession ? "视频通话中..." : "开始视频通话");
}
void MainWindow::updateCallButtonState()
{
    if(currentPeerHostNum == 0)
    {
        ui->btnVoiceChat->setEnabled(false);
        ui->btnVideoChat->setEnabled(false);
        return;
    }
    bool voiceReady = isWorkerReady(currentPeerHostNum, 2, ipRoute);
    bool videoReady = isWorkerReady(currentPeerHostNum, 3, ipRoute);
    ui->btnVoiceChat->setEnabled(voiceReady);
    bool hasActiveSession = !videoChatSessions.isEmpty();
    ui->btnVideoChat->setEnabled(videoReady && !hasActiveSession);
}

void MainWindow::onStartVideoChat()
{
    if(!enCoder)
    {
        (enCoder=new videoencoder)->moveToThread(trd[EN]=new QThread);
        trd[EN]->start();
        QMetaObject::invokeMethod(enCoder,"startFloodTimer",Qt::QueuedConnection);

        connect(enCoder, &videoencoder::sendEncodedFrame, this, [this](const QImage& localFrame, const QByteArray& encodedData){
            for(auto it = videoChatSessions.begin(); it != videoChatSessions.end(); ++it)
            {
                VideoChatWindow* videoWindow = it.value().first;
                if(videoWindow)
                    videoWindow->showLocalVideo(localFrame);
            }
            if(ipRoute)
                for(dcworker* worker:getCallingPeerWorkers(ipRoute,videoChatSessions.keys()))
                    QMetaObject::invokeMethod(worker, "sendVideoMsg", Qt::QueuedConnection,Q_ARG(const QByteArray&, encodedData));
        });

        connect(trd[EN],&QThread::finished,this,[this](){
            trd[EN]->deleteLater();
            if(enCoder)
            {
                delete(enCoder);
                enCoder=nullptr;
            }
            trd[EN]=nullptr;
        });
    }

    if (currentPeerHostNum == 0)
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    if(!isWorkerReady(currentPeerHostNum,3,ipRoute))
        return;

    dcworker* worker=getDcWorker(ipRoute,currentPeerHostNum,3);
    if(worker)
    {
        uint64_t requestTime=getRunningTime();
        QMetaObject::invokeMethod(worker,"sendVideoMsg",Qt::QueuedConnection,
            Q_ARG(const QByteArray&,createRequest(3,QString("对方(%1)发起了视频通话请求").arg(localHostName),requestTime)));
        mission.emplace(requestTime,this,[this,planedCallingHostNum=currentPeerHostNum]{
            if(ipRoute)
                if(ipRoute->contains(planedCallingHostNum))
                    confirmStartVideoChat(planedCallingHostNum);
        });
        ui->stateMsg->appendPlainText("已发送视频通话请求 => 待对方确认");
    }
    else
        ui->stateMsg->appendPlainText("向该Peer的连接数不足 无法进行视频通话 请尝试设置向该Peer的对外连接数为不小于3的数");
}

void MainWindow::confirmStartVideoChat(int callingHostNum)
{
    if (videoChatSessions.contains(callingHostNum))
    {
        VideoChatWindow* existingWindow = videoChatSessions.value(callingHostNum).first;
        if (existingWindow)
        {
            existingWindow->show();
            existingWindow->activateWindow();
        }
        return;
    }
    VideoChatWindow* videoWindow = new VideoChatWindow(this);
    connect(videoWindow, &VideoChatWindow::hangUpClicked, this, [this,callingHostNum]() {
        onEndVideoChat(callingHostNum);
    });
    videoChatSessions.insert(callingHostNum, qMakePair(videoWindow, nullptr));
    QString peerName = peerNames.value(callingHostNum, "未知");
    videoWindow->setPeerName(peerName);
    updateVideoChatButtonState();
    videoWindow->show();
    ui->stateMsg->appendPlainText(QString("已启动与 %1 的视频通话").arg(peerName));

    if(ipRoute && ipRoute->contains(callingHostNum))
    {
        QVector<dcworker*>& workers = (*ipRoute)[callingHostNum];
        if(workers.size() >= 4 && workers[3])
            workers[3]->isVideoCalling = true;
    }
}

void MainWindow::onEndVideoChat(int peerHostNum)
{
    if(ipRoute && ipRoute->contains(peerHostNum))
    {
        QVector<dcworker*>& workers = (*ipRoute)[peerHostNum];
        if(workers.size() >= 4 && workers[3])
        {
            workers[3]->isVideoCalling = false;
            uint64_t dummyTime = 0;
            QByteArray hangupMsg = createResponse(3, false, dummyTime);
            hangupMsg[2] = (char)20;
            QMetaObject::invokeMethod(workers[3], "sendVideoMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, hangupMsg));
        }
    }
    cleanupVideoChatSession(peerHostNum);
}

void MainWindow::onRemoteVideoHangup(int peerHostNum)
{
    //收到对方挂断通知/仅本地清理(对方已自行停止,不需要再通知对方)
    if(ipRoute && ipRoute->contains(peerHostNum))
    {
        QVector<dcworker*>& workers = (*ipRoute)[peerHostNum];
        if(workers.size() >= 4 && workers[3])
            workers[3]->isVideoCalling = false;
    }
    cleanupVideoChatSession(peerHostNum);
}

void MainWindow::cleanupVideoChatSession(int peerHostNum)
{
    if (!videoChatSessions.contains(peerHostNum))
        return;

    auto session = videoChatSessions.take(peerHostNum);
    VideoChatWindow* videoWindow = session.first;
    QThread* videoThread = session.second;

    if (videoWindow) {
        videoWindow->close();
        videoWindow->deleteLater();
    }

    if (videoThread && videoThread->isRunning()) {
        videoThread->quit();
        videoThread->wait(3000);
        if (videoThread->isRunning()) {
            videoThread->terminate();
        }
        videoThread->deleteLater();
    }

    updateVideoChatButtonState();

    QString peerName = peerNames.value(peerHostNum, "未知");
    ui->stateMsg->appendPlainText(QString("与 %1 的视频通话已结束").arg(peerName));
}

void MainWindow::closeVideoChatWindow()
{
    for (auto it = videoChatSessions.begin(); it != videoChatSessions.end(); ) {
        int peerHostNum = it.key();
        VideoChatWindow* videoWindow = it->first;
        QThread* videoThread = it->second;

        if(ipRoute && ipRoute->contains(peerHostNum))
        {
            QVector<dcworker*>& workers = (*ipRoute)[peerHostNum];
            if(workers.size() >= 4 && workers[3])
                workers[3]->isVideoCalling = false;
        }

        if (videoWindow) {
            videoWindow->close();
            videoWindow->deleteLater();
        }

        if (videoThread && videoThread->isRunning()) {
            videoThread->quit();
            videoThread->wait(1000);
            if (videoThread->isRunning()) {
                videoThread->terminate();
            }
            videoThread->deleteLater();
        }

        it = videoChatSessions.erase(it);
    }

    updateVideoChatButtonState();
}

//(5)音频通话有待后期实现 目前暂时没多余时间开发 已经做好的是发送audio请求
void MainWindow::onStartVoiceChat()
{
    if(currentPeerHostNum == 0)
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    if(!isWorkerReady(currentPeerHostNum, 2, ipRoute))
    {
        ui->stateMsg->appendPlainText("向该Peer的连接数不足 无法进行语音通话 请尝试设置向该Peer的对外连接数为不小于3的数");
        return;
    }

    dcworker* worker = getDcWorker(ipRoute, currentPeerHostNum, 2);
    if(worker)
    {
        uint64_t requestTime=getRunningTime();
        QMetaObject::invokeMethod(worker, "sendAudioMsg", Qt::QueuedConnection,
                                  Q_ARG(const QByteArray&, createRequest(2, QString("对方(%1)发起了语音通话请求").arg(localHostName),requestTime)));
        ui->stateMsg->appendPlainText("已发送语音通话请求 => 待对方确认");
    }
}

//(6)收到请求
void MainWindow::onTransferRequest(uint8_t requestFlag,uint64_t requestTime,const QString& explain,void* voidDCWorker)
{
    dcworker* worker = static_cast<dcworker*>(voidDCWorker);
    if(!worker)
        return;

    QString title;
    switch(requestFlag)
    {
        case 1: title = "文件传输请求"; break;
        case 2: title = "语音通话请求"; break;
        case 3: title = "视频通话请求"; break;
        default: title = "请求"; break;
    }

    bool accepted = requestDialog(title, explain, "同意", "拒绝");
    QByteArray response = createResponse(requestFlag, accepted, requestTime);

    switch(requestFlag)
    {
        case 0:
            QMetaObject::invokeMethod(worker, "sendFileMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response), Q_ARG(bool, false));
            break;
        case 2:
            QMetaObject::invokeMethod(worker, "sendAudioMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response));
            break;
        case 3:
            QMetaObject::invokeMethod(worker, "sendVideoMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response));
            break;
    }

    QString peerName = peerNames.value(worker->peerHostNum, "未知");
    ui->stateMsg->appendPlainText(QString("已%1来自 %2 的请求").arg(accepted ? "同意" : "拒绝").arg(peerName));

    //coor端同意视频通话后 也需要启动本地视频会话(弹出窗口+启动encoder)
    if(accepted && requestFlag == 3)
    {
        int callingHostNum = worker->peerHostNum;
        confirmStartVideoChat(callingHostNum);

        if(!enCoder)
        {
            (enCoder=new videoencoder)->moveToThread(trd[EN]=new QThread);
            trd[EN]->start();
            QMetaObject::invokeMethod(enCoder,"startFloodTimer",Qt::QueuedConnection);

            connect(enCoder, &videoencoder::sendEncodedFrame, this, [this](const QImage& localFrame, const QByteArray& encodedData){
                for(auto it = videoChatSessions.begin(); it != videoChatSessions.end(); ++it)
                {
                    VideoChatWindow* videoWindow = it.value().first;
                    if(videoWindow)
                        videoWindow->showLocalVideo(localFrame);
                }
                if(ipRoute)
                    for(dcworker* worker:getCallingPeerWorkers(ipRoute,videoChatSessions.keys()))
                        QMetaObject::invokeMethod(worker, "sendVideoMsg", Qt::QueuedConnection,Q_ARG(const QByteArray&, encodedData));
            });

            connect(trd[EN],&QThread::finished,this,[this](){
                trd[EN]->deleteLater();
                if(enCoder)
                {
                    delete(enCoder);
                    enCoder=nullptr;
                }
                trd[EN]=nullptr;
            });
        }
    }
}
