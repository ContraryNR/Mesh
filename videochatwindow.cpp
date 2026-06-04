#include "videochatwindow.h"
#include <QDebug>

VideoChatWindow::VideoChatWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("视频通话");
    resize(800, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    peerNameLabel = new QLabel("正在与对方视频通话...", this);
    peerNameLabel->setAlignment(Qt::AlignCenter);
    QFont font = peerNameLabel->font();
    font.setPointSize(14);
    font.setBold(true);
    peerNameLabel->setFont(font);
    mainLayout->addWidget(peerNameLabel);

    QHBoxLayout* videoLayout = new QHBoxLayout;

    QVBoxLayout* localLayout = new QVBoxLayout;
    QLabel* localTitle = new QLabel("本地预览", this);
    localTitle->setAlignment(Qt::AlignCenter);
    localVideoLabel = new QLabel(this);
    localVideoLabel->setMinimumSize(240, 180);
    localVideoLabel->setMaximumSize(240, 180);
    localVideoLabel->setStyleSheet("background-color: #1a1a1a; border: 2px solid #333;");
    localVideoLabel->setAlignment(Qt::AlignCenter);
    localVideoLabel->setText("等待摄像头...");
    localLayout->addWidget(localTitle);
    localLayout->addWidget(localVideoLabel, 0, Qt::AlignCenter);
    videoLayout->addLayout(localLayout);

    QLabel* spacer = new QLabel("  ", this);
    videoLayout->addWidget(spacer);

    QVBoxLayout* remoteLayout = new QVBoxLayout;
    QLabel* remoteTitle = new QLabel("对方画面", this);
    remoteTitle->setAlignment(Qt::AlignCenter);
    remoteVideoLabel = new QLabel(this);
    remoteVideoLabel->setMinimumSize(480, 360);
    remoteVideoLabel->setMaximumSize(480, 360);
    remoteVideoLabel->setStyleSheet("background-color: #1a1a1a; border: 2px solid #333;");
    remoteVideoLabel->setAlignment(Qt::AlignCenter);
    remoteVideoLabel->setText("等待对方视频...");
    remoteLayout->addWidget(remoteTitle);
    remoteLayout->addWidget(remoteVideoLabel, 0, Qt::AlignCenter);
    videoLayout->addLayout(remoteLayout);

    mainLayout->addLayout(videoLayout);

    hangUpButton = new QPushButton("结束通话", this);
    hangUpButton->setMinimumHeight(40);
    hangUpButton->setStyleSheet("background-color: #e74c3c; color: white; font-size: 14px; font-weight: bold; border: none; padding: 8px; border-radius: 4px;");
    connect(hangUpButton, &QPushButton::clicked, this, &VideoChatWindow::hangUpClicked);
    mainLayout->addWidget(hangUpButton);

    setLayout(mainLayout);
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
}

VideoChatWindow::~VideoChatWindow()
{
}

void VideoChatWindow::showLocalVideo(const QImage& frame)
{
    QImage scaled = frame.scaled(localVideoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    localVideoLabel->setPixmap(QPixmap::fromImage(scaled));
}

void VideoChatWindow::showRemoteVideo(const QImage& frame, int peerHostNum)
{
    QImage scaled = frame.scaled(remoteVideoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    remoteVideoLabel->setPixmap(QPixmap::fromImage(scaled));
}

void VideoChatWindow::setPeerName(const QString& name)
{
    peerNameLabel->setText(QString("正在与 %1 视频通话").arg(name));
}
