#include "videochatwindow.h"
#include <QDebug>

VideoChatWindow::VideoChatWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("视频通话");
    resize(800, 640);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    //--- 顶部:对端名字 + 通话时长(横向并列) ---
    QHBoxLayout* topRow = new QHBoxLayout;
    peerNameLabel = new QLabel("正在与对方视频通话...", this);
    peerNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont font = peerNameLabel->font();
    font.setPointSize(13);
    font.setBold(true);
    peerNameLabel->setFont(font);
    topRow->addWidget(peerNameLabel);

    durationLabel = new QLabel("00:00", this);
    durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont df = durationLabel->font();
    df.setPointSize(11);
    durationLabel->setFont(df);
    durationLabel->setStyleSheet("color: #888;");
    durationLabel->setMinimumWidth(72);
    topRow->addWidget(durationLabel);
    mainLayout->addLayout(topRow);

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

    //--- 底部:静音 + 结束通话(横向并排) ---
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    muteButton = new QPushButton("静音", this);
    muteButton->setCheckable(true);
    muteButton->setMinimumHeight(40);
    muteButton->setStyleSheet(
        "QPushButton { background-color: #34495e; color: white; font-size: 13px; "
        "font-weight: bold; border: none; padding: 8px; border-radius: 4px; }"
        "QPushButton:checked { background-color: #e67e22; }");
    connect(muteButton, &QPushButton::toggled, this, &VideoChatWindow::onMuteButtonClicked);
    btnRow->addWidget(muteButton);

    hangUpButton = new QPushButton("结束通话", this);
    hangUpButton->setMinimumHeight(40);
    hangUpButton->setStyleSheet("background-color: #e74c3c; color: white; font-size: 14px; font-weight: bold; border: none; padding: 8px; border-radius: 4px;");
    connect(hangUpButton, &QPushButton::clicked, this, &VideoChatWindow::hangUpClicked);
    btnRow->addWidget(hangUpButton);

    mainLayout->addLayout(btnRow);

    setLayout(mainLayout);
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);

    //--- duration 刷新 timer(放在主线程,1Hz 刷新) ---
    durationTimer = new QTimer(this);
    durationTimer->setInterval(1000);
    connect(durationTimer, &QTimer::timeout, this, &VideoChatWindow::onDurationTick);
    elapsed.invalidate();
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
    Q_UNUSED(peerHostNum);
    QImage scaled = frame.scaled(remoteVideoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    remoteVideoLabel->setPixmap(QPixmap::fromImage(scaled));
}

void VideoChatWindow::setPeerName(const QString& name)
{
    peerNameLabel->setText(QString("正在与 %1 视频通话").arg(name));
}

void VideoChatWindow::startDurationTimer()
{
    //(1)重置 elapsed 并启动 timer;从 0 开始累计
    //(2)只在本端弹窗上独立计时,不需要和对方同步
    elapsed.start();
    durationLabel->setText("00:00");
    if(!durationTimer->isActive())
        durationTimer->start();
}

void VideoChatWindow::setMuteState(bool muted)
{
    //(1)与 AudioChatWindow::setMuteState 同样的处理:仅同步按钮状态,不发信号
    if(muteButton->isChecked() == muted)
        return;
    muteButton->blockSignals(true);
    muteButton->setChecked(muted);
    muteButton->setText(muted ? "已静音" : "静音");
    muteButton->blockSignals(false);
    m_muted = muted;
}

void VideoChatWindow::onDurationTick()
{
    if(!elapsed.isValid())
        return;
    qint64 ms = elapsed.elapsed();
    int totalSec = static_cast<int>(ms / 1000);
    int mm = totalSec / 60;
    int ss = totalSec % 60;
    durationLabel->setText(QString("%1:%2")
                               .arg(mm, 2, 10, QChar('0'))
                               .arg(ss, 2, 10, QChar('0')));
}

void VideoChatWindow::onMuteButtonClicked()
{
    m_muted = muteButton->isChecked();
    muteButton->setText(m_muted ? "已静音" : "静音");
    emit muteToggled(m_muted);
}
