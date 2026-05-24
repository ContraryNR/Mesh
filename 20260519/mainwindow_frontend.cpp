#include "mainwindow.h"
#include "./ui_mainwindow.h"

//收信
/*->当前选中该Peer->直接插入信息
->当前未选中该Peer->插入信息到chatHistoryStringList下次切换到该Peer时插入全部聊天记录*/
//创建bubble(label)容器=>获取布局指针=>添加bubble到容器=>基于消息来源确定'bubble'和'弹簧'的插入顺序从而在bubbleContanier实现行内定位
//在chatListWidget中插入新item=>将上述单label容器设置为item内widget
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
//检查hostNum是否存在=>存入QHash.chatStringList=>若来信主机正好为选中(当前)主机则插入消息气泡
//依赖onPeerAdd和onPeerRemoved槽函数更新tableWidget
//hostName从tableWidget获取
//因此onPeerMsgReceived只需要接收hostNum从tableWidget进行查询即可
void MainWindow::onPeerMsgReceived(int peerHostNum,const QString& msg)
{
    if(!peerNames.contains(peerHostNum))
        return;
    peerChatHistory[peerHostNum].append(QString("peer|%1").arg(msg));
    if(currentPeerHostNum == peerHostNum)
        addChatBubble(ui->chatListWidget, msg, false, false);
}
//清空当前聊天区chatWidget->遍历currentHost聊天记录并(先判断消息源)插入当前聊天区
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
//切换peer时刷新聊天区(点击空白处不会触发itemClicked信号)=>同时更新类成员currentPeerHostNum
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
    }
}
/*###################################################################################################################*/
void MainWindow::onAttachFile()
{
    if(!currentPeerHostNum)return;
    QString filePath = QFileDialog::getOpenFileName(this, "选择文件", "", "所有文件 (*.*)");
    if(!filePath.isEmpty())
    {
        ui->stateMsg->appendPlainText(QString("选中文件: %1").arg(filePath));
        filesender* fileSender;QThread* trd;
        fileSenderContanier.insert(fileSender=new filesender(filePath),trd=new QThread);
        fileSender->moveToThread(trd);trd->start();fileSender->running=true;
        QMetaObject::invokeMethod(fileSender,"sendFile",Qt::QueuedConnection,Q_ARG(void*,(void*)dcManager),Q_ARG(int,currentPeerHostNum));
        connect(fileSender,&filesender::fileSendFinish,this,[fileSender,trd](){
            fileSender->deleteLater();
            trd->quit();
        });
        connect(trd,&QThread::finished,trd,&QThread::deleteLater);
    }
}
/*###################################################################################################################*/
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
    if(!peerChatHistory.contains(peerHostNum))
        peerChatHistory.insert(peerHostNum, QStringList());
    ui->btnBroadcast->setEnabled(true);
}
//传入被移除的peer的hostNum=>从tableWidget遍历查询同主机号item=>查到即移除整行=>若正好是目前选中的peer则清空当前chatWidget
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
        //当前聊天页为空时禁用文件发送按钮
        ui->btnSend->setEnabled(false);
    }
    if(peerNames.size()==0)
        ui->btnBroadcast->setEnabled(false);
}
void MainWindow::goSendUnicastMsg()
{
    QString msg = ui->sendingMsg->text();
    if(msg.isEmpty()) return;
    if(currentPeerHostNum == 0)
    {
        ui->sendingMsg->clear();
        return;
    }
    QMetaObject::invokeMethod(dcManager, "sendStringToPeer", Qt::QueuedConnection,
                              Q_ARG(int, currentPeerHostNum),
                              Q_ARG(const QString&, msg));
    peerChatHistory[currentPeerHostNum].append(QString("self|%1").arg(msg));
    addChatBubble(ui->chatListWidget, msg, true, false);
    ui->sendingMsg->clear();
}
void MainWindow::goSendBroadcastMsg()
{
    QString msg = ui->sendingMsg->text();
    msg.push_front(QTime::currentTime().toString("HH:mm")+QString("\n"));
    if(msg.isEmpty()) return;
    if(peerNames.isEmpty()) {
        ui->sendingMsg->clear();
        return;
    }
    QMetaObject::invokeMethod(dcManager, "broadcastString", Qt::QueuedConnection,
                              Q_ARG(const QString&, msg));
    QString historyItem = QString("self|broadcast|%1").arg(msg);
    for(int hostNum : peerNames.keys()) {
        peerChatHistory[hostNum].append(historyItem);
    }
    if(currentPeerHostNum != 0)
        addChatBubble(ui->chatListWidget, msg, true, true);
    ui->sendingMsg->clear();
}
