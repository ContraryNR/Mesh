/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEST_H
#define UI_TEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *mainLayout;
    QGroupBox *hostInfoGroup;
    QHBoxLayout *hostInfoLayout;
    QLabel *hostNameLabel;
    QLabel *hostNameDisplay;
    QLabel *modeLabel;
    QLabel *modeDisplay;
    QHBoxLayout *configLayout;
    QGroupBox *coordinatorGroup;
    QHBoxLayout *coordConfigLayout;
    QLabel *listenIPLabel;
    QComboBox *listenIP;
    QLabel *listenPortLabel;
    QSpinBox *listenPort;
    QGroupBox *peerGroup;
    QHBoxLayout *coordAddrLayout;
    QLabel *coordIPLabel;
    QLineEdit *coordIP;
    QLabel *coordPortLabel;
    QSpinBox *coordPort;
    QGroupBox *vnetGroup;
    QHBoxLayout *vnetLayout;
    QLabel *subnetLabel;
    QLineEdit *subnetPrefix;
    QLabel *networkLenLabel;
    QSpinBox *networkLen;
    QLabel *localIPLabel;
    QLabel *localIP;
    QHBoxLayout *buttonLayout;
    QPushButton *btnStart;
    QPushButton *btnShut;
    QPushButton *btnSettings;
    QHBoxLayout *mainContentLayout;
    QGroupBox *peerTableGroup;
    QVBoxLayout *peerTableLayout;
    QTableWidget *peerTable;
    QVBoxLayout *chatLayout;
    QGroupBox *chatGroup;
    QVBoxLayout *chatBoxLayout;
    QLabel *chatTitle;
    QListWidget *chatListWidget;
    QHBoxLayout *sendLayout;
    QPushButton *btnAttach;
    QLineEdit *sendingMsg;
    QPushButton *btnSend;
    QPushButton *btnBroadcast;
    QGroupBox *fileTransferGroup;
    QVBoxLayout *fileTransferLayout;
    QTableWidget *fileTransferTable;
    QHBoxLayout *bottomLayout;
    QGroupBox *trafficGroup;
    QHBoxLayout *trafficLayout;
    QLabel *internalSpeed;
    QLabel *externalSpeed;
    QLabel *pendingStackSize;
    QHBoxLayout *clearLayout;
    QPushButton *cleanState;
    QPushButton *cleanMessage;
    QPlainTextEdit *stateMsg;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1200, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        mainLayout = new QVBoxLayout(centralwidget);
        mainLayout->setSpacing(6);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(10, 10, 10, 10);
        hostInfoGroup = new QGroupBox(centralwidget);
        hostInfoGroup->setObjectName("hostInfoGroup");
        hostInfoLayout = new QHBoxLayout(hostInfoGroup);
        hostInfoLayout->setObjectName("hostInfoLayout");
        hostNameLabel = new QLabel(hostInfoGroup);
        hostNameLabel->setObjectName("hostNameLabel");

        hostInfoLayout->addWidget(hostNameLabel);

        hostNameDisplay = new QLabel(hostInfoGroup);
        hostNameDisplay->setObjectName("hostNameDisplay");

        hostInfoLayout->addWidget(hostNameDisplay);

        modeLabel = new QLabel(hostInfoGroup);
        modeLabel->setObjectName("modeLabel");

        hostInfoLayout->addWidget(modeLabel);

        modeDisplay = new QLabel(hostInfoGroup);
        modeDisplay->setObjectName("modeDisplay");

        hostInfoLayout->addWidget(modeDisplay);


        mainLayout->addWidget(hostInfoGroup);

        configLayout = new QHBoxLayout();
        configLayout->setObjectName("configLayout");
        coordinatorGroup = new QGroupBox(centralwidget);
        coordinatorGroup->setObjectName("coordinatorGroup");
        coordConfigLayout = new QHBoxLayout(coordinatorGroup);
        coordConfigLayout->setObjectName("coordConfigLayout");
        listenIPLabel = new QLabel(coordinatorGroup);
        listenIPLabel->setObjectName("listenIPLabel");

        coordConfigLayout->addWidget(listenIPLabel);

        listenIP = new QComboBox(coordinatorGroup);
        listenIP->setObjectName("listenIP");

        coordConfigLayout->addWidget(listenIP);

        listenPortLabel = new QLabel(coordinatorGroup);
        listenPortLabel->setObjectName("listenPortLabel");

        coordConfigLayout->addWidget(listenPortLabel);

        listenPort = new QSpinBox(coordinatorGroup);
        listenPort->setObjectName("listenPort");
        listenPort->setMinimum(1);
        listenPort->setMaximum(65535);
        listenPort->setValue(2345);

        coordConfigLayout->addWidget(listenPort);


        configLayout->addWidget(coordinatorGroup);

        peerGroup = new QGroupBox(centralwidget);
        peerGroup->setObjectName("peerGroup");
        coordAddrLayout = new QHBoxLayout(peerGroup);
        coordAddrLayout->setObjectName("coordAddrLayout");
        coordIPLabel = new QLabel(peerGroup);
        coordIPLabel->setObjectName("coordIPLabel");

        coordAddrLayout->addWidget(coordIPLabel);

        coordIP = new QLineEdit(peerGroup);
        coordIP->setObjectName("coordIP");

        coordAddrLayout->addWidget(coordIP);

        coordPortLabel = new QLabel(peerGroup);
        coordPortLabel->setObjectName("coordPortLabel");

        coordAddrLayout->addWidget(coordPortLabel);

        coordPort = new QSpinBox(peerGroup);
        coordPort->setObjectName("coordPort");
        coordPort->setMinimum(1);
        coordPort->setMaximum(65535);
        coordPort->setValue(2345);

        coordAddrLayout->addWidget(coordPort);


        configLayout->addWidget(peerGroup);

        vnetGroup = new QGroupBox(centralwidget);
        vnetGroup->setObjectName("vnetGroup");
        vnetLayout = new QHBoxLayout(vnetGroup);
        vnetLayout->setObjectName("vnetLayout");
        subnetLabel = new QLabel(vnetGroup);
        subnetLabel->setObjectName("subnetLabel");

        vnetLayout->addWidget(subnetLabel);

        subnetPrefix = new QLineEdit(vnetGroup);
        subnetPrefix->setObjectName("subnetPrefix");

        vnetLayout->addWidget(subnetPrefix);

        networkLenLabel = new QLabel(vnetGroup);
        networkLenLabel->setObjectName("networkLenLabel");

        vnetLayout->addWidget(networkLenLabel);

        networkLen = new QSpinBox(vnetGroup);
        networkLen->setObjectName("networkLen");
        networkLen->setMinimum(1);
        networkLen->setMaximum(31);
        networkLen->setValue(8);

        vnetLayout->addWidget(networkLen);

        localIPLabel = new QLabel(vnetGroup);
        localIPLabel->setObjectName("localIPLabel");

        vnetLayout->addWidget(localIPLabel);

        localIP = new QLabel(vnetGroup);
        localIP->setObjectName("localIP");

        vnetLayout->addWidget(localIP);


        configLayout->addWidget(vnetGroup);


        mainLayout->addLayout(configLayout);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        btnStart = new QPushButton(centralwidget);
        btnStart->setObjectName("btnStart");

        buttonLayout->addWidget(btnStart);

        btnShut = new QPushButton(centralwidget);
        btnShut->setObjectName("btnShut");

        buttonLayout->addWidget(btnShut);

        btnSettings = new QPushButton(centralwidget);
        btnSettings->setObjectName("btnSettings");

        buttonLayout->addWidget(btnSettings);


        mainLayout->addLayout(buttonLayout);

        mainContentLayout = new QHBoxLayout();
        mainContentLayout->setSpacing(10);
        mainContentLayout->setObjectName("mainContentLayout");
        peerTableGroup = new QGroupBox(centralwidget);
        peerTableGroup->setObjectName("peerTableGroup");
        peerTableLayout = new QVBoxLayout(peerTableGroup);
        peerTableLayout->setObjectName("peerTableLayout");
        peerTable = new QTableWidget(peerTableGroup);
        if (peerTable->columnCount() < 4)
            peerTable->setColumnCount(4);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        peerTable->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        peerTable->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        peerTable->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        peerTable->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        peerTable->setObjectName("peerTable");
        peerTable->setMinimumSize(QSize(250, 0));
        peerTable->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        peerTable->setAlternatingRowColors(true);
        peerTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

        peerTableLayout->addWidget(peerTable);


        mainContentLayout->addWidget(peerTableGroup);

        chatLayout = new QVBoxLayout();
        chatLayout->setSpacing(0);
        chatLayout->setObjectName("chatLayout");
        chatGroup = new QGroupBox(centralwidget);
        chatGroup->setObjectName("chatGroup");
        chatBoxLayout = new QVBoxLayout(chatGroup);
        chatBoxLayout->setSpacing(0);
        chatBoxLayout->setObjectName("chatBoxLayout");
        chatTitle = new QLabel(chatGroup);
        chatTitle->setObjectName("chatTitle");
        chatTitle->setStyleSheet(QString::fromUtf8("font-weight: bold; color: #333;"));

        chatBoxLayout->addWidget(chatTitle);

        chatListWidget = new QListWidget(chatGroup);
        chatListWidget->setObjectName("chatListWidget");
        chatListWidget->setMinimumSize(QSize(400, 250));
        chatListWidget->setFrameShape(QFrame::NoFrame);
        chatListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        chatBoxLayout->addWidget(chatListWidget);


        chatLayout->addWidget(chatGroup);

        sendLayout = new QHBoxLayout();
        sendLayout->setSpacing(5);
        sendLayout->setObjectName("sendLayout");
        btnAttach = new QPushButton(centralwidget);
        btnAttach->setObjectName("btnAttach");
        btnAttach->setFixedWidth(40);

        sendLayout->addWidget(btnAttach);

        sendingMsg = new QLineEdit(centralwidget);
        sendingMsg->setObjectName("sendingMsg");

        sendLayout->addWidget(sendingMsg);

        btnSend = new QPushButton(centralwidget);
        btnSend->setObjectName("btnSend");

        sendLayout->addWidget(btnSend);

        btnBroadcast = new QPushButton(centralwidget);
        btnBroadcast->setObjectName("btnBroadcast");

        sendLayout->addWidget(btnBroadcast);


        chatLayout->addLayout(sendLayout);


        mainContentLayout->addLayout(chatLayout);

        fileTransferGroup = new QGroupBox(centralwidget);
        fileTransferGroup->setObjectName("fileTransferGroup");
        fileTransferLayout = new QVBoxLayout(fileTransferGroup);
        fileTransferLayout->setObjectName("fileTransferLayout");
        fileTransferTable = new QTableWidget(fileTransferGroup);
        if (fileTransferTable->columnCount() < 4)
            fileTransferTable->setColumnCount(4);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        fileTransferTable->setHorizontalHeaderItem(0, __qtablewidgetitem4);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        fileTransferTable->setHorizontalHeaderItem(1, __qtablewidgetitem5);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        fileTransferTable->setHorizontalHeaderItem(2, __qtablewidgetitem6);
        QTableWidgetItem *__qtablewidgetitem7 = new QTableWidgetItem();
        fileTransferTable->setHorizontalHeaderItem(3, __qtablewidgetitem7);
        fileTransferTable->setObjectName("fileTransferTable");
        fileTransferTable->setMinimumSize(QSize(300, 0));
        fileTransferTable->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        fileTransferTable->setAlternatingRowColors(true);
        fileTransferTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

        fileTransferLayout->addWidget(fileTransferTable);


        mainContentLayout->addWidget(fileTransferGroup);


        mainLayout->addLayout(mainContentLayout);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName("bottomLayout");
        trafficGroup = new QGroupBox(centralwidget);
        trafficGroup->setObjectName("trafficGroup");
        trafficLayout = new QHBoxLayout(trafficGroup);
        trafficLayout->setObjectName("trafficLayout");
        internalSpeed = new QLabel(trafficGroup);
        internalSpeed->setObjectName("internalSpeed");

        trafficLayout->addWidget(internalSpeed);

        externalSpeed = new QLabel(trafficGroup);
        externalSpeed->setObjectName("externalSpeed");

        trafficLayout->addWidget(externalSpeed);

        pendingStackSize = new QLabel(trafficGroup);
        pendingStackSize->setObjectName("pendingStackSize");

        trafficLayout->addWidget(pendingStackSize);


        bottomLayout->addWidget(trafficGroup);

        clearLayout = new QHBoxLayout();
        clearLayout->setObjectName("clearLayout");
        cleanState = new QPushButton(centralwidget);
        cleanState->setObjectName("cleanState");

        clearLayout->addWidget(cleanState);

        cleanMessage = new QPushButton(centralwidget);
        cleanMessage->setObjectName("cleanMessage");

        clearLayout->addWidget(cleanMessage);


        bottomLayout->addLayout(clearLayout);

        stateMsg = new QPlainTextEdit(centralwidget);
        stateMsg->setObjectName("stateMsg");
        stateMsg->setMinimumSize(QSize(300, 60));
        stateMsg->setReadOnly(true);

        bottomLayout->addWidget(stateMsg);


        mainLayout->addLayout(bottomLayout);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1200, 21));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "QNetLink", nullptr));
        hostInfoGroup->setTitle(QCoreApplication::translate("MainWindow", "\346\234\254\346\234\272\344\277\241\346\201\257", nullptr));
        hostNameLabel->setText(QCoreApplication::translate("MainWindow", "\344\270\273\346\234\272\345\220\215", nullptr));
        hostNameDisplay->setText(QCoreApplication::translate("MainWindow", "(\347\255\211\345\276\205\350\256\276\347\275\256)", nullptr));
        modeLabel->setText(QCoreApplication::translate("MainWindow", "\346\250\241\345\274\217", nullptr));
        modeDisplay->setText(QCoreApplication::translate("MainWindow", "(\347\255\211\345\276\205\350\256\276\347\275\256)", nullptr));
        coordinatorGroup->setTitle(QCoreApplication::translate("MainWindow", "\345\215\217\350\260\203\350\200\205\351\205\215\347\275\256", nullptr));
        listenIPLabel->setText(QCoreApplication::translate("MainWindow", "\347\233\221\345\220\254\345\234\260\345\235\200", nullptr));
        listenPortLabel->setText(QCoreApplication::translate("MainWindow", "\347\253\257\345\217\243", nullptr));
        peerGroup->setTitle(QCoreApplication::translate("MainWindow", "\350\212\202\347\202\271\351\205\215\347\275\256", nullptr));
        coordIPLabel->setText(QCoreApplication::translate("MainWindow", "\345\215\217\350\260\203\350\200\205\345\234\260\345\235\200", nullptr));
        coordIP->setPlaceholderText(QCoreApplication::translate("MainWindow", "\345\215\217\350\260\203\350\200\205IP", nullptr));
        coordPortLabel->setText(QCoreApplication::translate("MainWindow", "\347\253\257\345\217\243", nullptr));
        vnetGroup->setTitle(QCoreApplication::translate("MainWindow", "\350\231\232\346\213\237\347\275\221\345\215\241", nullptr));
        subnetLabel->setText(QCoreApplication::translate("MainWindow", "\347\275\221\346\256\265\345\211\215\347\274\200", nullptr));
        subnetPrefix->setText(QCoreApplication::translate("MainWindow", "10.0.0", nullptr));
        networkLenLabel->setText(QCoreApplication::translate("MainWindow", "\347\275\221\347\273\234\344\275\215", nullptr));
        localIPLabel->setText(QCoreApplication::translate("MainWindow", "\346\234\254\346\234\272\350\231\232\346\213\237\345\234\260\345\235\200", nullptr));
        localIP->setText(QCoreApplication::translate("MainWindow", "(\350\207\252\345\212\250\345\210\206\351\205\215)", nullptr));
        btnStart->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\220\257\347\273\204\347\275\221", nullptr));
        btnShut->setText(QCoreApplication::translate("MainWindow", "\345\201\234\346\255\242\347\273\204\347\275\221", nullptr));
        btnSettings->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256", nullptr));
        peerTableGroup->setTitle(QCoreApplication::translate("MainWindow", "\345\267\262\350\277\236\346\216\245\350\212\202\347\202\271", nullptr));
        QTableWidgetItem *___qtablewidgetitem = peerTable->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "\345\272\217\345\217\267", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = peerTable->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "\344\270\273\346\234\272\345\220\215", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = peerTable->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "\350\231\232\346\213\237\345\234\260\345\235\200", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = peerTable->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        chatGroup->setTitle(QCoreApplication::translate("MainWindow", "\350\201\212\345\244\251", nullptr));
        chatTitle->setText(QCoreApplication::translate("MainWindow", "\346\234\252\351\200\211\346\213\251\350\201\212\345\244\251\345\257\271\350\261\241", nullptr));
        btnAttach->setText(QCoreApplication::translate("MainWindow", "\360\237\223\216", nullptr));
#if QT_CONFIG(tooltip)
        btnAttach->setToolTip(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201\346\226\207\344\273\266", nullptr));
#endif // QT_CONFIG(tooltip)
        sendingMsg->setPlaceholderText(QCoreApplication::translate("MainWindow", "\350\276\223\345\205\245\346\266\210\346\201\257...", nullptr));
        btnSend->setText(QCoreApplication::translate("MainWindow", "\345\215\225\346\222\255\345\217\221\351\200\201", nullptr));
        btnBroadcast->setText(QCoreApplication::translate("MainWindow", "\345\271\277\346\222\255\345\217\221\351\200\201", nullptr));
        fileTransferGroup->setTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266\344\274\240\350\276\223", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = fileTransferTable->horizontalHeaderItem(0);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266\345\220\215", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = fileTransferTable->horizontalHeaderItem(1);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("MainWindow", "\345\244\247\345\260\217", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = fileTransferTable->horizontalHeaderItem(2);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        QTableWidgetItem *___qtablewidgetitem7 = fileTransferTable->horizontalHeaderItem(3);
        ___qtablewidgetitem7->setText(QCoreApplication::translate("MainWindow", "\345\210\233\345\273\272\346\227\266\351\227\264", nullptr));
        trafficGroup->setTitle(QCoreApplication::translate("MainWindow", "\346\265\201\351\207\217", nullptr));
        internalSpeed->setText(QCoreApplication::translate("MainWindow", "\345\205\245\347\253\231: --", nullptr));
        externalSpeed->setText(QCoreApplication::translate("MainWindow", "\345\207\272\347\253\231: --", nullptr));
        pendingStackSize->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\217\221\347\247\257\345\216\213: --", nullptr));
        cleanState->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\347\212\266\346\200\201", nullptr));
        cleanMessage->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\350\201\212\345\244\251", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEST_H
