#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QInputDialog>
#include <QFileInfo>
#include <QProgressBar>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMediaDevices>
#include <QJsonObject>
#include "settingsdialog.h"

//(0)dcManger->MainWindow同步更新 和 peerTable切换item逻辑
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
    QMetaObject::invokeMethod(getDcWorker((void*)ipRoute,currentPeerHostNum,TYPE_TUN), "sendStringMsg", Qt::QueuedConnection,Q_ARG(const QString&, msg));
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
        dcworker* worker=getDcWorker((void*)ipRoute,currentPeerHostNum,TYPE_FILE);
        qLog()<<"[FILE-UI] onAttachFile peerHostNum="<<currentPeerHostNum<<" file="<<fileName<<" worker="<<worker;

        uint64_t requestTime=getRunningTime();
        qLog()<<"[FILE-UI] 发送文件请求 requestTime="<<requestTime;
        //(1)wire 格式:msgType(1) + neogotieteType(1) + neogotiateSate(1) + requestTime(8) + json(N)
        //(2)requestTime 走独立 8 字节 wire 字段(不进 json)
        //(3)json 内含 explain/fileName/fileSize(原 explain 字符串描述 fileName 的方式已结构化)
        QJsonObject requestJson;
        requestJson["explain"]  = QString("对方%1请求发送文件'%2'给你").arg(localHostName).arg(fileName);
        requestJson["fileName"] = fileName;
        requestJson["fileSize"] = (qint64)fileInfo.size();
        QMetaObject::invokeMethod(worker,"sendFileMsg",Qt::QueuedConnection,
        Q_ARG(const QByteArray&,createRequest(TYPE_FILE,requestTime,requestJson)),Q_ARG(bool,false));

        mission.emplace(requestTime,this,
                        [this,filePath,fileInfo,fileName,worker]()
            {
                qLog()<<"[FILE-UI] mission回调触发! worker="<<worker;
                QPointer<dcworker> safeDcWorker(worker);
                if(!safeDcWorker)
                {
                    qLog()<<"[FILE-UI] worker已被销毁,中止";
                    return;
                }

                FileTransferInfo transferInfo(fileName,fileInfo.size(),QDateTime::currentDateTime(),false);
                fileTransferHash[currentPeerHostNum].append(transferInfo);

                filesender* fileSender;QThread* trd;
                fileSenderContanier.insert(fileSender=new filesender(filePath),trd=new QThread);
                fileSender->moveToThread(trd);trd->start();fileSender->running=true;
                qLog()<<"[FILE-UI] filesender已创建, invokeMethod sendFile";

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
                    delete(fileSender);
                    qLog()<<"fileSender已销毁 senderTrd已退出事件循环 将被OS回收";
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
//(4.0)视频通话参数弹窗:分辨率(宽x高)+帧率,均 spinBox;同时嵌入音频参数(下拉列表)
//     弹窗确认后:写入 pendingVideoWidth/Height/Fps 与 pendingAudioSampleRate/ChannelCount
//     之后由 onStartVideoChat 把它拼到 request 内发给对端
//     返回 true 表示用户点了"确认"
//(3.1 / 3.3)视频通话参数弹窗(分辨率+帧率 spinBox,音频采样率/通道数 comboBox)
//  - outW/outH/outFps/outAsr/outAcc/outDev:调用方传入引用,确认后写入;取消则原值不变
bool MainWindow::popVideoCallParamsDialog(int& outW,int& outH,int& outFps,int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate)
{
    QDialog dlg(this);
    dlg.setWindowTitle("视频通话参数设置");
    dlg.setMinimumWidth(320);

    QFormLayout* form = new QFormLayout(&dlg);

    //--- 视频参数(spinBox) ---
    QSpinBox* sbWidth = new QSpinBox(&dlg);
    sbWidth->setRange(160, 1920); sbWidth->setSingleStep(160);
    sbWidth->setValue(outW>0?outW:640);
    QSpinBox* sbHeight = new QSpinBox(&dlg);
    sbHeight->setRange(120, 1080); sbHeight->setSingleStep(120);
    sbHeight->setValue(outH>0?outH:480);
    QSpinBox* sbFps = new QSpinBox(&dlg);
    sbFps->setRange(5, 60); sbFps->setValue(outFps>0?outFps:15);

    QHBoxLayout* resLayout = new QHBoxLayout;
    resLayout->addWidget(sbWidth);
    resLayout->addWidget(new QLabel("x", &dlg));
    resLayout->addWidget(sbHeight);
    resLayout->addWidget(new QLabel("@", &dlg));
    resLayout->addWidget(sbFps);
    resLayout->addWidget(new QLabel("fps", &dlg));

    QWidget* resWidget = new QWidget(&dlg);
    resWidget->setLayout(resLayout);
    form->addRow("分辨率 / 帧率:", resWidget);

    //--- 音频参数(comboBox) ---
    QComboBox* cbDev = new QComboBox(&dlg);
    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    for(int i = 0; i < inputDevices.size(); ++i)
        cbDev->addItem(inputDevices[i].description(), i);
    if(cbDev->count() == 0)
    {
        QMessageBox::warning(this, "提示", "未检测到可用的音频输入设备");
        return false;
    }
    cbDev->setCurrentIndex(0);

    QComboBox* cbSr = new QComboBox(&dlg);
    cbSr->addItem("8000",  8000);
    cbSr->addItem("16000", 16000);
    cbSr->addItem("24000", 24000);
    cbSr->addItem("48000", 48000);
    cbSr->setCurrentIndex(cbSr->findData(outAsr>0?outAsr:48000));

    QComboBox* cbCh = new QComboBox(&dlg);
    cbCh->addItem("单声道 (1)", 1);
    cbCh->addItem("立体声 (2)", 2);
    cbCh->setCurrentIndex(cbCh->findData(outAcc>0?outAcc:1));

    QGroupBox* audioGrp = new QGroupBox("音频参数(视频通话也启用音频,以下参数会一并协商)", &dlg);
    QFormLayout* audioForm = new QFormLayout(audioGrp);
    audioForm->addRow("输入设备:", cbDev);
    audioForm->addRow("采样率:", cbSr);
    audioForm->addRow("通道数:", cbCh);

    QComboBox* cbNoise = new QComboBox(&dlg);
    cbNoise->addItem("关闭 (0)", 0);
    cbNoise->addItem("低 (1)", 1);
    cbNoise->addItem("默认 (2)", 2);
    cbNoise->addItem("高 (5)", 5);
    cbNoise->addItem("很高 (10)", 10);
    cbNoise->setCurrentIndex(cbNoise->findData(outNoiseGate));
    audioForm->addRow("噪声门:", cbNoise);

    form->addRow(audioGrp);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(dlg.exec() != QDialog::Accepted)
        return false;

    outW   = sbWidth->value();
    outH   = sbHeight->value();
    outFps = sbFps->value();
    int devIdx = cbDev->currentData().toInt();
    outDev = inputDevices[devIdx];
    outAsr = cbSr->currentData().toInt();
    outAcc = cbCh->currentData().toInt();
    outNoiseGate = cbNoise->currentData().toInt();
    return true;
}

//(3.2)音频通话参数弹窗: device + sampleRate + channelCount,均 comboBox
bool MainWindow::popAudioCallParamsDialog(int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate)
{
    QDialog dlg(this);
    dlg.setWindowTitle("音频通话参数设置");
    dlg.setMinimumWidth(320);

    QFormLayout* form = new QFormLayout(&dlg);

    QComboBox* cbDev = new QComboBox(&dlg);
    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    for(int i = 0; i < inputDevices.size(); ++i)
        cbDev->addItem(inputDevices[i].description(), i);
    if(cbDev->count() == 0)
    {
        QMessageBox::warning(this, "提示", "未检测到可用的音频输入设备");
        return false;
    }
    cbDev->setCurrentIndex(0);

    QComboBox* cbSr = new QComboBox(&dlg);
    cbSr->addItem("8000",  8000);
    cbSr->addItem("16000", 16000);
    cbSr->addItem("24000", 24000);
    cbSr->addItem("48000", 48000);
    cbSr->setCurrentIndex(cbSr->findData(outAsr>0?outAsr:48000));

    QComboBox* cbCh = new QComboBox(&dlg);
    cbCh->addItem("单声道 (1)", 1);
    cbCh->addItem("立体声 (2)", 2);
    cbCh->setCurrentIndex(cbCh->findData(outAcc>0?outAcc:1));

    QComboBox* cbNoise = new QComboBox(&dlg);
    cbNoise->addItem("关闭 (0)", 0);
    cbNoise->addItem("低 (1)", 1);
    cbNoise->addItem("默认 (2)", 2);
    cbNoise->addItem("高 (5)", 5);
    cbNoise->addItem("很高 (10)", 10);
    cbNoise->setCurrentIndex(cbNoise->findData(outNoiseGate));

    form->addRow("输入设备:", cbDev);
    form->addRow("采样率:", cbSr);
    form->addRow("通道数:", cbCh);
    form->addRow("噪声门:", cbNoise);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(dlg.exec() != QDialog::Accepted)
        return false;

    int devIdx = cbDev->currentData().toInt();
    outDev = inputDevices[devIdx];
    outAsr = cbSr->currentData().toInt();
    outAcc = cbCh->currentData().toInt();
    outNoiseGate = cbNoise->currentData().toInt();
    return true;
}

//(6.1)富文本 request 弹窗:question 用粗体(显眼),params 用等宽字体(正常显示)
bool MainWindow::requestDialogRich(const QString& title,const QString& question,const QString& paramsText,
                                   const QString& btnText1,const QString& btnText0)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    QString richText = QString("<p style=\"font-weight:bold;font-size:11pt;\">%1</p>").arg(question.toHtmlEscaped());
    if(!paramsText.isEmpty())
    {
        //params 用 <pre> 等宽字体显示,正常字号
        richText += QString("<pre style=\"font-family:Consolas,monospace;font-size:10pt;"
                            "background:#F5F5F5;padding:6px;\">%1</pre>").arg(paramsText.toHtmlEscaped());
    }
    msgBox.setText(richText);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setIcon(QMessageBox::Question);
    QPushButton* button1 = msgBox.addButton(btnText1, QMessageBox::ActionRole);
    QPushButton* button0 = msgBox.addButton(btnText0, QMessageBox::ActionRole);
    msgBox.setDefaultButton(button1);
    msgBox.exec();
    return (QPushButton*)(msgBox.clickedButton()) == button1 ? 1 : 0;
}

//通话请求弹窗:显示对方参数 + 本端可编辑参数 + 音频设备选择
//video 模式:可修改 vw/vh/vf/asr/acc + 设备
//audio 模式:可修改 asr/acc + 设备
//返回 true=同意,各 out 参数为用户最终选择的值
bool MainWindow::requestDialogWithDevice(const QString& title,const QString& question,const QString& paramsText,
                                          int msgType,int& outVw,int& outVh,int& outVf,int& outAsr,int& outAcc,QAudioDevice& outDev,int& outNoiseGate)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setMinimumWidth(360);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);

    QLabel* titleLabel = new QLabel(question, &dlg);
    QFont f = titleLabel->font();
    f.setBold(true);
    titleLabel->setFont(f);
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);

    //显示对方原始参数(只读提示)
    if(!paramsText.isEmpty())
    {
        QLabel* paramsLabel = new QLabel("对方参数:\n" + paramsText, &dlg);
        QFont mono("Consolas", 10);
        mono.setStyleHint(QFont::Monospace);
        paramsLabel->setFont(mono);
        paramsLabel->setStyleSheet("background:#F5F5F5; padding:6px;");
        paramsLabel->setAlignment(Qt::AlignLeft);
        mainLayout->addWidget(paramsLabel);
    }

    //--- 视频参数(仅 video 类型) ---
    QSpinBox* sbWidth = nullptr;
    QSpinBox* sbHeight = nullptr;
    QSpinBox* sbFps = nullptr;
    if(msgType == TYPE_VIDEO)
    {
        QGroupBox* videoGrp = new QGroupBox("本端视频参数(可修改)", &dlg);
        QFormLayout* videoForm = new QFormLayout(videoGrp);
        sbWidth = new QSpinBox(&dlg);
        sbWidth->setRange(160, 1920); sbWidth->setSingleStep(160);
        sbWidth->setValue(outVw>0?outVw:640);
        sbHeight = new QSpinBox(&dlg);
        sbHeight->setRange(120, 1080); sbHeight->setSingleStep(120);
        sbHeight->setValue(outVh>0?outVh:480);
        sbFps = new QSpinBox(&dlg);
        sbFps->setRange(5, 60); sbFps->setValue(outVf>0?outVf:15);
        videoForm->addRow("宽度:", sbWidth);
        videoForm->addRow("高度:", sbHeight);
        videoForm->addRow("帧率:", sbFps);
        mainLayout->addWidget(videoGrp);
    }

    //--- 音频设备 + 参数(audio/video 类型) ---
    QComboBox* cbDev = new QComboBox(&dlg);
    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    for(int i = 0; i < inputDevices.size(); ++i)
        cbDev->addItem(inputDevices[i].description(), i);
    if(cbDev->count() == 0)
    {
        QMessageBox::warning(this, "提示", "未检测到可用的音频输入设备");
        return false;
    }
    cbDev->setCurrentIndex(0);

    QComboBox* cbSr = new QComboBox(&dlg);
    cbSr->addItem("8000",  8000);
    cbSr->addItem("16000", 16000);
    cbSr->addItem("24000", 24000);
    cbSr->addItem("48000", 48000);
    cbSr->setCurrentIndex(cbSr->findData(outAsr>0?outAsr:48000));

    QComboBox* cbCh = new QComboBox(&dlg);
    cbCh->addItem("单声道 (1)", 1);
    cbCh->addItem("立体声 (2)", 2);
    cbCh->setCurrentIndex(cbCh->findData(outAcc>0?outAcc:1));

    QGroupBox* audioGrp = new QGroupBox("本端音频参数(可修改)", &dlg);
    QFormLayout* audioForm = new QFormLayout(audioGrp);
    audioForm->addRow("输入设备:", cbDev);
    audioForm->addRow("采样率:", cbSr);
    audioForm->addRow("通道数:", cbCh);

    QComboBox* cbNoise = new QComboBox(&dlg);
    cbNoise->addItem("关闭 (0)", 0);
    cbNoise->addItem("低 (1)", 1);
    cbNoise->addItem("默认 (2)", 2);
    cbNoise->addItem("高 (5)", 5);
    cbNoise->addItem("很高 (10)", 10);
    cbNoise->setCurrentIndex(cbNoise->findData(outNoiseGate));
    audioForm->addRow("噪声门:", cbNoise);

    mainLayout->addWidget(audioGrp);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText("同意");
    btns->button(QDialogButtonBox::Cancel)->setText("拒绝");
    mainLayout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(dlg.exec() != QDialog::Accepted)
        return false;

    if(sbWidth)  outVw = sbWidth->value();
    if(sbHeight) outVh = sbHeight->value();
    if(sbFps)    outVf = sbFps->value();
    outAsr = cbSr->currentData().toInt();
    outAcc = cbCh->currentData().toInt();
    outNoiseGate = cbNoise->currentData().toInt();
    int devIdx = cbDev->currentData().toInt();
    if(devIdx >= 0 && devIdx < inputDevices.size())
        outDev = inputDevices[devIdx];
    return true;
}

void MainWindow::onDecodedFrame(const QImage & frameImg, int peerHostNum)
{
    if (!videoChatSessions.contains(peerHostNum))
    {
        //(1)这里是"已存在 enCoder 但还没有这个 peerHostNum 的窗口"的分支,不需要再启动 audio pipeline
        //(2)参数传 0 即可(confirmStartVideoChat 仅在 enCoder==nullptr 时才用到这些参数)
        confirmStartVideoChat(peerHostNum,0,0,0,0,0,QAudioDevice());
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
    bool voiceReady = isWorkerReady(currentPeerHostNum, TYPE_AUDIO, ipRoute);
    bool videoReady = isWorkerReady(currentPeerHostNum, TYPE_VIDEO, ipRoute);
    ui->btnVoiceChat->setEnabled(voiceReady);
    bool hasActiveSession = !videoChatSessions.isEmpty();
    ui->btnVideoChat->setEnabled(videoReady && !hasActiveSession);
}

void MainWindow::onStartVideoChat()
{
    if (currentPeerHostNum == 0)
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    if(!isWorkerReady(currentPeerHostNum,TYPE_VIDEO,ipRoute))
    {
        ui->stateMsg->appendPlainText("向该Peer的连接数不足 无法进行视频通话 请尝试设置向该Peer的对外连接数为不小于3的数");
        return;
    }

    //(3.1 + 3.3)先弹窗,让用户选择视频参数 + 音频参数(视频通话也启用音频,所以要协商)
    //     refactor: 改为 stack 局部变量,不再写回 mainwindow 成员
    int vw=640,vh=480,vf=15,asr=48000,acc=1,noiseGate=2;
    QAudioDevice audioDev;
    if(!popVideoCallParamsDialog(vw,vh,vf,asr,acc,audioDev,noiseGate))
        return;//用户取消

    //探测本地音频设备实际支持的格式
    QAudioDevice dev = audioDev.isNull() ? QMediaDevices::defaultAudioInput() : audioDev;
    QAudioFormat probeFmt;
    probeFmt.setSampleRate(asr);
    probeFmt.setChannelCount(acc);
    probeFmt.setSampleFormat(QAudioFormat::Int16);
    if(!dev.isFormatSupported(probeFmt))
        probeFmt = dev.preferredFormat();
    int actualAsr = probeFmt.sampleRate();
    int actualAcc = probeFmt.channelCount();

    dcworker* worker=getDcWorker(ipRoute,currentPeerHostNum,TYPE_VIDEO);
    if(worker)
    {
        uint64_t requestTime=getRunningTime();
        QJsonObject requestJson;
        requestJson["explain"]            = QString("对方(%1)发起了视频通话请求").arg(localHostName);
        requestJson["videoWidth"]         = vw;
        requestJson["videoHeight"]        = vh;
        requestJson["videoFps"]           = vf;
        requestJson["audioSampleRate"]    = actualAsr;
        requestJson["audioChannelCount"]  = actualAcc;
        QMetaObject::invokeMethod(worker,"sendVideoMsg",Qt::QueuedConnection,
            Q_ARG(const QByteArray&,createRequest(TYPE_VIDEO, requestTime, requestJson)));
        mission.emplace(requestTime,this,
            [this,planedCallingHostNum=currentPeerHostNum,vw,vh,vf,actualAsr,actualAcc,audioDev,noiseGate]{
                if(ipRoute)
                    if(ipRoute->contains(planedCallingHostNum))
                        confirmStartVideoChat(planedCallingHostNum,vw,vh,vf,actualAsr,actualAcc,audioDev,noiseGate);
            });
        ui->stateMsg->appendPlainText(QString("已发送视频通话请求 => 待对方确认 视频:%1x%2@%3fps 音频:%4Hz/%5ch")
                                      .arg(vw).arg(vh).arg(vf).arg(actualAsr).arg(actualAcc));
    }
    else
        ui->stateMsg->appendPlainText("向该Peer的连接数不足 无法进行视频通话 请尝试设置向该Peer的对外连接数为不小于3的数");
}

void MainWindow::confirmStartVideoChat(int callingHostNum,int vw,int vh,int vf,int asr,int acc,const QAudioDevice& audioDev,int noiseGate)
{
    //(新增)把当前 video 会话的对端也登记为 audioCalleeHostNum
    //   - audioCap 的本端音量回调通过 audioCalleeHostNum 投递
    //   - 视频通话内嵌音频,本端 MIC 录到的是"和 video peer 通话时"的声音
    audioCalleeHostNum = callingHostNum;
    if(!enCoder)
    {
        //(3.1)用弹窗协商的视频参数建 encoder
        (enCoder=new videoencoder(0, vw, vh, vf))
            ->moveToThread(trd[EN]=new QThread);
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
    }
    //(3.3)视频通话同时启用音频:如果本地 audio pipeline 还没建,按协商参数建
    //     注意:仅在首次启动视频会话时建(已有的复用,避免多次连线时重复)
    if(!audioCap && !audioEnCoder && !audioPlayer)
    {
        (audioCap=new audiocapture(asr, acc, 20, audioDev, noiseGate))
            ->moveToThread(trd[AU]=new QThread);
        int actualSr = audioCap->sampleRate;
        int actualCh = audioCap->channelCount;
        (audioEnCoder=new audioencoder(actualSr, actualCh, 32000))
            ->moveToThread(trd[AU]);
        audioPlayer = new audioplayer(actualSr, actualCh);
        audioPlayer->moveToThread(trd[AU]);
        trd[AU]->start();
        QMetaObject::invokeMethod(audioCap,"startFloodTimer",Qt::QueuedConnection);
        QMetaObject::invokeMethod(audioPlayer,"startPlayback",Qt::QueuedConnection);

        //capture -> encoder
        connect(audioCap, &audiocapture::sendPcmFrame, audioEnCoder, &audioencoder::encodeFlood);
        //(新增)本端每帧音量:从 trd[AU] 跨线程发到主线程,推到 VideoChatWindow / AudioChatWindow
        //   - 这里在 video 创建 audio pipeline 的分支也要连,否则视频通话时静音不工作
        //   - audioCap 同一时刻只服务一个 session,直接读 audioCalleeHostNum(已在调用 confirmStartVideoChat 之前/之中被更新)
        connect(audioCap, &audiocapture::sendPcmLevel, this,
                [this](int level){
                    if(audioCalleeHostNum != 0)
                        onLocalAudioLevel(level, audioCalleeHostNum);
                });
        //encoder -> 当前 callingHostNum 对应的 worker[2]
        dcworker* audioWorker = (ipRoute && ipRoute->contains(callingHostNum))
            ? getDcWorker(ipRoute, callingHostNum, TYPE_AUDIO) : nullptr;
        if(audioWorker)
        {
            connect(audioEnCoder, &audioencoder::sendEncodedAudio, this, [audioWorker](const QByteArray& encodedData){
                QMetaObject::invokeMethod(audioWorker, "sendAudioMsg", Qt::QueuedConnection,
                                          Q_ARG(const QByteArray&, encodedData));
            });
        }
        //inbound decoded PCM -> player
        connect(this, &MainWindow::inboundDecodedAudio, audioPlayer, &audioplayer::playFlood);
        qLog()<<"[video+audio] audio pipeline started sr="<<asr
              <<"ch="<<acc;
    }
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
    //(新增)视频弹窗的"静音"按钮:转发到 mainwindow::onAudioChatMuteToggled
    //   - 视频通话与音频通话共用 audioCap(都是同一 trd[AU] 上的 capture/encoder/player)
    //   - 这里只把 mute 状态透传过去,不直接操作 audioCap,保持单一控制点
    connect(videoWindow, &VideoChatWindow::muteToggled, this, [this,callingHostNum](bool muted){
        onAudioChatMuteToggled(callingHostNum, muted);
    });
    videoChatSessions.insert(callingHostNum, qMakePair(videoWindow, nullptr));
    QString peerName = peerNames.value(callingHostNum, "未知");
    videoWindow->setPeerName(peerName);
    //(新增)启动通话时长计时;默认非静音(新通话初始)
    videoWindow->setMuteState(false);
    videoWindow->startDurationTimer();
    updateVideoChatButtonState();
    videoWindow->show();
    ui->stateMsg->appendPlainText(QString("已启动与 %1 的视频通话 视频:%2x%3@%4fps 音频:%5Hz/%6ch")
                                  .arg(peerName).arg(vw).arg(vh).arg(vf).arg(asr).arg(acc));

    if(ipRoute && ipRoute->contains(callingHostNum))
    {
        QVector<dcworker*>& workers = (*ipRoute)[callingHostNum];
        if(workers.size() >= 4 && workers[3])
            workers[3]->isVideoCalling = true;
        //(3.3)视频通话内嵌音频,把 audio worker 的 isAudioCalling 也置位
        if(workers.size() >= 3 && workers[2])
            workers[2]->isAudioCalling = true;
    }
}

void MainWindow::onEndVideoChat(int peerHostNum)
{
    qLog()<<"[onEndVideoChat] peerHostNum="<<peerHostNum;
    if(ipRoute && ipRoute->contains(peerHostNum))
    {
        QVector<dcworker*>& workers = (*ipRoute)[peerHostNum];
        if(workers.size() >= 4 && workers[3])
        {
            workers[3]->isVideoCalling = false;
            uint64_t dummyTime = 0;
            QByteArray hangupMsg = createResponse(TYPE_VIDEO, false, dummyTime);
            hangupMsg[2] = (char)20;
            QMetaObject::invokeMethod(workers[3], "sendVideoMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, hangupMsg));
        }
    }
    cleanupVideoChatSession(peerHostNum);
}

void MainWindow::onRemoteVideoHangup(int peerHostNum)
{
    qLog()<<"[onRemoteVideoHangup] peerHostNum="<<peerHostNum;
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
    qLog()<<"[cleanup] cleanupVideoChatSession called, peerHostNum="<<peerHostNum;
    if (!videoChatSessions.contains(peerHostNum))
    {
        qLog()<<"[cleanup] session not found, returning";
        return;
    }

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

    //(新增)视频会话结束,清掉 audioCalleeHostNum(若此 peer 也是 audio 会话)
    //   - 视频窗口关闭时,本端本帧音量若仍在发(尽管 audioCap 还在)就不会误投递到错弹窗
    //   - audioCap 的 mute 状态保持(下一次会话由用户自行决定是否取消静音)
    if(audioCalleeHostNum == peerHostNum)
        audioCalleeHostNum = 0;


    qLog()<<"[cleanup] sessions.isEmpty()="<<videoChatSessions.isEmpty()<<" enCoder="<<enCoder<<" trd[EN]="<<trd[EN];
    if(videoChatSessions.isEmpty()&&enCoder)
    {
        qLog()<<"[cleanup] invokeMethod shutdown...";
        QMetaObject::invokeMethod(enCoder,"shutdown",Qt::BlockingQueuedConnection);
        qLog()<<"[cleanup] quit...";
        trd[EN]->quit();
        qLog()<<"[cleanup] wait...";
        trd[EN]->wait();
        qLog()<<"[cleanup] wait returned, deleting enCoder...";
        delete enCoder;enCoder=nullptr;
        qLog()<<"[cleanup] deleting trd[EN]...";
        delete trd[EN];trd[EN]=nullptr;
        qLog()<<"[cleanup] done";
    }

    //(新增 H8)视频通话结束时,检查 audio pipeline 是否被泄漏
    //   - confirmStartVideoChat 会建 audioCap/Encoder/Player(共享同一 trd[AU])
    //   - cleanupVideoChatSession 此前只清视频,不碰 audio,导致 audioCap 持续在线
    //   - 视频通话建立的 audio 没在 audioChatSessions 里登记(只有 confirmStartAudioChat 才会 insert)
    //   - 因此这里用"peer 不在 audioChatSessions 中 且 audioCap 还活着"判定"audio 是 video 建的,该释放"
    // 视频通话建立的 audio pipeline 未在 audioChatSessions 登记,
    // 需要在最后一个视频会话结束时释放;但如果仍有纯音频会话在用则不释放
    if(audioCap && videoChatSessions.isEmpty() && audioChatSessions.isEmpty())
    {
        cleanupAudioChatPipeline();
    }
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

    // 视频全部关闭后,若 audio pipeline 仅由 video 维护(不在 audioChatSessions 中),
    // 需一并释放
    if(audioCap && audioChatSessions.isEmpty())
    {
        cleanupAudioChatPipeline();
    }
}

//(5)音频通话
void MainWindow::onStartAudioChat()
{
    if(currentPeerHostNum == 0)
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    if(!isWorkerReady(currentPeerHostNum, TYPE_AUDIO, ipRoute))
    {
        ui->stateMsg->appendPlainText("向该Peer的连接数不足 无法进行语音通话 请尝试设置向该Peer的对外连接数为不小于3的数");
        return;
    }

    //(3.2)先弹窗: device + sampleRate + channelCount
    //     refactor: 改为 stack 局部变量
    int asr=48000,acc=1,noiseGate=2;
    QAudioDevice audioDev;
    if(!popAudioCallParamsDialog(asr,acc,audioDev,noiseGate))
        return;

    //探测本地设备实际支持的格式(弹窗参数可能不被设备支持,需回退)
    //把实际参数发给对方协商,避免两端格式不一致
    QAudioDevice dev = audioDev.isNull() ? QMediaDevices::defaultAudioInput() : audioDev;
    QAudioFormat probeFmt;
    probeFmt.setSampleRate(asr);
    probeFmt.setChannelCount(acc);
    probeFmt.setSampleFormat(QAudioFormat::Int16);
    if(!dev.isFormatSupported(probeFmt))
        probeFmt = dev.preferredFormat();
    int actualAsr = probeFmt.sampleRate();
    int actualAcc = probeFmt.channelCount();

    dcworker* worker = getDcWorker(ipRoute, currentPeerHostNum, TYPE_AUDIO);
    if(worker)
    {
        uint64_t requestTime=getRunningTime();
        QJsonObject requestJson;
        requestJson["explain"]            = QString("对方(%1)发起了语音通话请求").arg(localHostName);
        requestJson["audioSampleRate"]    = actualAsr;
        requestJson["audioChannelCount"]  = actualAcc;

        QMetaObject::invokeMethod(worker, "sendAudioMsg", Qt::QueuedConnection,
                                  Q_ARG(const QByteArray&, createRequest(TYPE_AUDIO, requestTime, requestJson)));
        mission.emplace(requestTime,this,
            [this,planedCallingHostNum=currentPeerHostNum,actualAsr,actualAcc,audioDev,noiseGate]{
                if(ipRoute)
                    if(ipRoute->contains(planedCallingHostNum))
                        confirmStartAudioChat(planedCallingHostNum,actualAsr,actualAcc,audioDev,noiseGate);
            });
        ui->stateMsg->appendPlainText(QString("已发送语音通话请求 => 待对方确认 音频:%1Hz/%2ch")
                                      .arg(actualAsr).arg(actualAcc));
    }
}

//(5-1)音频会话被对方/自己接受 后启动会话(由 onTransferRequest 和 mission lambda 共同调用)
//   asr/acc/audioDev:从弹窗或 signal 携带的参数传入,直接用于构造本地 audio pipeline
void MainWindow::confirmStartAudioChat(int callingHostNum,int asr,int acc,const QAudioDevice& audioDev,int noiseGate)
{
    audioChatSessions.insert(callingHostNum);
    audioCalleeHostNum = callingHostNum;
    if(ipRoute && ipRoute->contains(callingHostNum))
    {
        QVector<dcworker*>& workers=(*ipRoute)[callingHostNum];
        if(workers.size()>=3 && workers[2])
            workers[2]->isAudioCalling=true;
    }

    //(3.2)按协商参数建本地 audio pipeline
    if(!audioCap && !audioEnCoder && !audioPlayer)
    {
        (audioCap=new audiocapture(asr, acc, 20, audioDev, noiseGate))
            ->moveToThread(trd[AU]=new QThread);
        int actualSr = audioCap->sampleRate;
        int actualCh = audioCap->channelCount;
        (audioEnCoder=new audioencoder(actualSr, actualCh, 32000))
            ->moveToThread(trd[AU]);
        audioPlayer = new audioplayer(actualSr, actualCh);
        audioPlayer->moveToThread(trd[AU]);
        trd[AU]->start();
        QMetaObject::invokeMethod(audioCap,"startFloodTimer",Qt::QueuedConnection);
        QMetaObject::invokeMethod(audioPlayer,"startPlayback",Qt::QueuedConnection);

        //capture -> encoder
        connect(audioCap, &audiocapture::sendPcmFrame, audioEnCoder, &audioencoder::encodeFlood);
        //(2.5)本端每帧音量:从 trd[AU] 跨线程发到主线程,推到 AudioChatWindow
        //   - 这里把 peerHostNum 闭包进 lambda:由于 audio pipeline 同一时刻只服务一路通话,
        //     暂时用 audioCalleeHostNum 也可,但更稳妥是用 capture/encoder 当时正在为之服务的那路
        //   - 视频通话场景下也会调到这个分支(confirmStartVideoChat 里建的 audio pipeline)
        connect(audioCap, &audiocapture::sendPcmLevel, this,
                [this](int level){
                    //audioCap 同一时刻只服务一个 session,直接读 audioCalleeHostNum
                    //(避免把本端音量推到错误对端的弹窗上)
                    if(audioCalleeHostNum != 0)
                        onLocalAudioLevel(level, audioCalleeHostNum);
                });
        //encoder -> audio worker
        dcworker* audioWorker = (ipRoute && ipRoute->contains(callingHostNum))
            ? getDcWorker(ipRoute, callingHostNum, TYPE_AUDIO) : nullptr;
        if(audioWorker)
        {
            connect(audioEnCoder, &audioencoder::sendEncodedAudio, this, [audioWorker](const QByteArray& encodedData){
                QMetaObject::invokeMethod(audioWorker, "sendAudioMsg", Qt::QueuedConnection,
                                          Q_ARG(const QByteArray&, encodedData));
            });
        }
        //inbound decoded PCM -> player
        connect(this, &MainWindow::inboundDecodedAudio, audioPlayer, &audioplayer::playFlood);
        qLog()<<"[audio] audio pipeline started sr="<<asr
              <<"ch="<<acc;
    }

    //(5-1.1)创建/复用 AudioChatWindow 弹窗(类似音乐播放器)
    //   - 同一对端多次进入/离开,复用同一个弹窗
    //   - 创建后:挂 hangUpClicked / muteToggled 信号;把对端名字写上;启动 duration timer
    if(!audioChatWindows.contains(callingHostNum))
    {
        AudioChatWindow* audioWindow = new AudioChatWindow(this);
        connect(audioWindow, &AudioChatWindow::hangUpClicked, this, [this, callingHostNum](){
            onEndAudioChat(callingHostNum);
        });
        connect(audioWindow, &AudioChatWindow::muteToggled, this, [this, callingHostNum](bool muted){
            onAudioChatMuteToggled(callingHostNum, muted);
        });
        audioChatWindows.insert(callingHostNum, audioWindow);
    }
    AudioChatWindow* audioWindow = audioChatWindows.value(callingHostNum);
    audioWindow->setPeerName(peerNames.value(callingHostNum, QString("Host_%1").arg(callingHostNum)));
    audioWindow->setMuteState(false);//新会话默认非静音
    audioWindow->startDurationTimer();
    audioWindow->show();
    audioWindow->raise();
    audioWindow->activateWindow();

    ui->stateMsg->appendPlainText(QString("已与 %1 建立音频通话 音频:%2Hz/%3ch")
                                  .arg(peerNames.value(callingHostNum,"未知"))
                                  .arg(asr).arg(acc));
    //audio 会话按钮改为 toggle 状态(挂断按钮 = 再次点击)
    updateCallButtonState();
}

//(5-2)主动挂断音频会话(本端点击"结束音频通话"按钮触发)
void MainWindow::onEndAudioChat(int peerHostNum)
{
    qLog()<<"[onEndAudioChat] peerHostNum="<<peerHostNum;
    //(1)向对端发送 neogotiate20 (state=20) 通知对端
    //(2)立刻本地清理(不等对方 ack,本地优先)
    if(ipRoute && ipRoute->contains(peerHostNum))
    {
        QVector<dcworker*>& workers=(*ipRoute)[peerHostNum];
        if(workers.size()>=3 && workers[2])
        {
            workers[2]->isAudioCalling=false;
            uint64_t dummyTime=0;
            QByteArray hangupMsg=createResponse(TYPE_AUDIO,false,dummyTime);
            hangupMsg[2]=(char)20;//state=20 表示挂断(中断)
            QMetaObject::invokeMethod(workers[2],"sendAudioMsg",Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&,hangupMsg));
        }
    }
    cleanupAudioChatSession(peerHostNum);
}

//(5-3)对方发来 neogotiate20 通知(由 dcmanager::audioHangupReceived 转发)
void MainWindow::onRemoteAudioHangup(int peerHostNum)
{
    qLog()<<"[onRemoteAudioHangup] peerHostNum="<<peerHostNum;
    if(ipRoute && ipRoute->contains(peerHostNum))
    {
        QVector<dcworker*>& workers=(*ipRoute)[peerHostNum];
        if(workers.size()>=3 && workers[2])
            workers[2]->isAudioCalling=false;
    }
    cleanupAudioChatSession(peerHostNum);
}

//(5-4)清理 audio 通话会话(本地释放 audioCap/Encoder/Player + UI 提示)
//     - 同时被 onEndAudioChat(主动挂断) 和 onRemoteAudioHangup(对方挂断) 调用
//     - 如果当前同时存在视频通话(没挂),则 audio pipeline 不应被释放(它可能由 video 维护)
void MainWindow::cleanupAudioChatSession(int peerHostNum)
{
    qLog()<<"[cleanup] cleanupAudioChatSession called, peerHostNum="<<peerHostNum;
    if(!audioChatSessions.contains(peerHostNum))
    {
        //即使不在 set 里(可能视频通话也用了 audio pipeline),也允许继续 release
        qLog()<<"[cleanup-audio] session not in set, may be video-owned";
    }
    audioChatSessions.remove(peerHostNum);
    //(5-4.1)关闭/销毁对端对应的 AudioChatWindow
    //   - 注意:这里只动 audioChatWindows 里的对应项;不要顺手关掉视频弹窗
    if(audioChatWindows.contains(peerHostNum))
    {
        AudioChatWindow* audioWindow = audioChatWindows.take(peerHostNum);
        if(audioWindow)
        {
            audioWindow->close();
            audioWindow->deleteLater();
        }
    }
    //(1)如果视频会话还在,音频 pipeline 由视频复用,这里只清掉本 peer 的状态位(已经在调用方清过)
    //(2)只有当 audioChatSessions 彻底空掉 且 没有视频通话在使用 audio 时,才释放 audio pipeline
    bool videoActive = !videoChatSessions.isEmpty();
    if(audioChatSessions.isEmpty() && !videoActive)
    {
        cleanupAudioChatPipeline();
    }
    if(audioCalleeHostNum == peerHostNum)
        audioCalleeHostNum = 0;
    QString peerName = peerNames.value(peerHostNum,"未知");
    ui->stateMsg->appendPlainText(QString("与 %1 的音频通话已结束").arg(peerName));
    updateCallButtonState();
}

//(5-5)真正释放 audioCap/Encoder/Player 及其线程(异步,不阻塞主线程)
void MainWindow::cleanupAudioChatPipeline()
{
    qLog()<<"[cleanup-audio] release audioCap/Encoder/Player (async)";

    //先标记指针为 nullptr,防止重入;实际对象延迟释放
    auto cap = audioCap;
    auto enc = audioEnCoder;
    auto player = audioPlayer;
    auto trdAU = trd[AU];
    audioCap = nullptr;
    audioEnCoder = nullptr;
    audioPlayer = nullptr;
    trd[AU] = nullptr;

    //异步 shutdown:在 trd[AU] 上执行资源释放(停止 timer/音频设备),不阻塞主线程
    if(cap)
        QMetaObject::invokeMethod(cap, "shutdown", Qt::QueuedConnection);
    if(enc)
        QMetaObject::invokeMethod(enc, "shutdown", Qt::QueuedConnection);
    if(player)
        QMetaObject::invokeMethod(player, "shutdown", Qt::QueuedConnection);

    //异步释放对象:在各自线程的下次事件循环中 deleteLater
    if(cap)     cap->deleteLater();
    if(enc)     enc->deleteLater();
    if(player)  player->deleteLater();

    //线程清理:quit() 告诉事件循环退出;finished 后 deleteLater 线程自身
    if(trdAU)
    {
        connect(trdAU, &QThread::finished, trdAU, [trdAU](){
            trdAU->deleteLater();
        });
        trdAU->quit();
    }
}

//(5-3)对端过来的音频帧解码完成 -> 推到 player 线程
void MainWindow::onDecodedAudioFlood(const QByteArray& pcmData,int peerHostNum)
{
    Q_UNUSED(peerHostNum);
    emit inboundDecodedAudio(pcmData);
}

//(5-3.1)对端音量:从 dcmanager 推过来,投递到对应 peer 的 AudioChatWindow
//   - 主线程槽(paintEvent 等都在主线程)
//   - 若该 peer 没有弹窗(可能视频通话共用),不做任何事(视频弹窗暂不显示对端音量条)
void MainWindow::onRemoteAudioLevel(int level,int peerHostNum)
{
    if(!audioChatWindows.contains(peerHostNum))
        return;
    AudioChatWindow* audioWindow = audioChatWindows.value(peerHostNum);
    if(audioWindow)
        audioWindow->pushRemoteLevel(level);
}

//(5-3.2)本端音量:从 audiocapture 推过来,投递到对应 peer 的 AudioChatWindow
//   - 静音时 floodTimer 已停,不会再进 timeout,所以 level 也不会送来
//   - 这是期望行为:弹窗上的本端音量条在静音时不抖动
void MainWindow::onLocalAudioLevel(int level,int peerHostNum)
{
    if(!audioChatWindows.contains(peerHostNum))
        return;
    AudioChatWindow* audioWindow = audioChatWindows.value(peerHostNum);
    if(audioWindow)
        audioWindow->pushLocalLevel(level);
}

//(5-3.3)静音按钮回调:转发到 audiocapture::setMuted(在 trd[AU] 上执行)
//   - video 弹窗的 mute 也会走这里(共用 audioCap)
//   - video 和 audio 同一时刻共用一个 audioCap,只设一次
void MainWindow::onAudioChatMuteToggled(int callingPeerHostNum,bool muted)
{
    Q_UNUSED(callingPeerHostNum);
    if(!audioCap)
        return;
    //跨线程:audioCap 在 trd[AU],通过 QueuedConnection 投递
    QMetaObject::invokeMethod(audioCap, "setMuted", Qt::QueuedConnection,
                              Q_ARG(bool, muted));
    ui->stateMsg->appendPlainText(muted ? "[静音] 本端停止向对端发送音频" : "[取消静音] 恢复发送音频");
}

//(6)收到请求
void MainWindow::onTransferRequest(uint8_t msgType,uint64_t requestTime,const QJsonObject& callParams,void* voidDCWorker)
{
    qLog()<<"[FILE-UI] onTransferRequest msgType="<<msgType
          <<" requestTime="<<requestTime
          <<" hasCallParams="<<!callParams.isEmpty();
    dcworker* worker = static_cast<dcworker*>(voidDCWorker);
    if(!worker)
        return;

    QString title;
    switch(msgType)
    {
        case TYPE_FILE:  title = "文件传输请求"; break;
        case TYPE_AUDIO: title = "语音通话请求"; break;
        case TYPE_VIDEO: title = "视频通话请求"; break;
        default: title = "请求"; break;
    }

    //(1)explain / 类型协商参数都从 callParams 直接按 msgType 取
    //   - callParams 为空(json 缺失/解析失败):explain 空字符串,参数全部 0
    //(2)audio 通话只取 asr/acc;video 通话取 5 个全部;file 通话取 fileName/fileSize
    QString explain=callParams.value("explain").toString();
    int vw=0,vh=0,vf=0,asr=0,acc=0;
    vw=callParams.value("videoWidth").toInt();
    vh=callParams.value("videoHeight").toInt();
    vf=callParams.value("videoFps").toInt();
    asr=callParams.value("audioSampleRate").toInt();
    acc=callParams.value("audioChannelCount").toInt();

    QString paramsText;
    if(msgType == TYPE_VIDEO)
    {
        paramsText = QString("视频:%1 x %2 @ %3 fps\n音频:%4 Hz / %5 ch")
                         .arg(vw>0?vw:0).arg(vh>0?vh:0).arg(vf>0?vf:0)
                         .arg(asr>0?asr:0).arg(acc>0?acc:0);
    }
    else if(msgType == TYPE_AUDIO)
    {
        paramsText = QString("音频:%1 Hz / %2 ch")
                         .arg(asr>0?asr:0).arg(acc>0?acc:0);
    }
    else if(msgType == TYPE_FILE)
    {
        QString fileName=callParams.value("fileName").toString();
        qint64 fileSize=callParams.value("fileSize").toVariant().toLongLong();
        paramsText = QString("文件:%1\n大小:%2 字节")
                         .arg(fileName.isEmpty()?"(未知)":fileName)
                         .arg(fileSize);
    }

    bool accepted = false;
    QAudioDevice selectedDevice;
    int selectedNoiseGate = 2;
    if(msgType == TYPE_AUDIO || msgType == TYPE_VIDEO)
        accepted = requestDialogWithDevice(title, explain.isEmpty() ? title : explain, paramsText,
                                           msgType, vw, vh, vf, asr, acc, selectedDevice, selectedNoiseGate);
    else
        accepted = requestDialogRich(title, explain.isEmpty() ? title : explain, paramsText, "同意", "拒绝");
    QByteArray response = createResponse(msgType, accepted, requestTime);

    switch(msgType)
    {
        case TYPE_FILE:
            QMetaObject::invokeMethod(worker, "sendFileMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response), Q_ARG(bool, false));
            break;
        case TYPE_AUDIO:
            QMetaObject::invokeMethod(worker, "sendAudioMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response));
            break;
        case TYPE_VIDEO:
            QMetaObject::invokeMethod(worker, "sendVideoMsg", Qt::QueuedConnection,
                                      Q_ARG(const QByteArray&, response));
            break;
    }

    QString peerName = peerNames.value(worker->peerHostNum, "未知");
    ui->stateMsg->appendPlainText(QString("已%1来自 %2 的请求").arg(accepted ? "同意" : "拒绝").arg(peerName));

    //(a)receiver 端 accept 后的本地启动路径:
    //   (1)把协商参数按消息类型写入对应 dcworker 的 std::atomic<int> 成员(直接 store,无需 invokeMethod)
    //       - audio 通话只填 asr/acc,video 通话填 vw/vh/vf + asr/acc
    //   (2)用本地的 stack 拷贝(刚才从 callParams 拿到的)调 confirmStart* 启动本地 pipeline
    if(accepted)
    {
        int callingHostNum = worker->peerHostNum;
        if(msgType == TYPE_AUDIO)
        {
            confirmStartAudioChat(callingHostNum, asr, acc, selectedDevice, selectedNoiseGate);
        }
        else if(msgType == TYPE_VIDEO)
        {
            confirmStartVideoChat(callingHostNum, vw, vh, vf, asr, acc, selectedDevice, selectedNoiseGate);
        }
    }
}
