#include "audiochatwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDateTime>

//============================================================
// AudioLevelBar:自绘对端/本端说话强度曲线(简易"音乐播放器"风格)
//============================================================
AudioLevelBar::AudioLevelBar(QWidget* parent)
    : QWidget(parent)
{
    m_levels.fill(0, kBarCount);
    //固定小高度,弹窗里只需要一小段
    setMinimumHeight(48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    //(1)暗色背景 + 浅灰边框,与视频通话的暗色风格保持一致
    setStyleSheet("background-color: #1a1a1a; border: 1px solid #333; border-radius: 4px;");
}

void AudioLevelBar::pushLevel(int level)
{
    if(level < 0) level = 0;
    if(level > 100) level = 100;
    m_levels[m_head] = level;
    m_head = (m_head + 1) % kBarCount;
    update();//触发 paintEvent
}

void AudioLevelBar::resetLevels()
{
    m_levels.fill(0, kBarCount);
    m_head = 0;
    update();
}

void AudioLevelBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);//柱状图不需要抗锯齿,关掉更快

    const int w = width();
    const int h = height();
    //(1)柱体宽度:总宽除以柱数,留 1px 间隙
    int barWidth = w / kBarCount;
    if(barWidth < 1) barWidth = 1;
    int gap = (w - barWidth * kBarCount) / kBarCount;
    if(gap < 0) gap = 0;

    //(2)从最旧到最新顺序绘制:leftmost = 最旧,rightmost = 最新
    for(int i = 0; i < kBarCount; ++i)
    {
        //环形索引:i=0 是最旧的那一格(m_head 处),依次往后
        int idx = (m_head + i) % kBarCount;
        int level = m_levels[idx];
        int barH = (level * (h - 4)) / 100;
        int x = i * (barWidth + gap);
        int y = h - barH - 2;//底部对齐,留 2px 内边距

        //(3)颜色按强度渐变:低=绿,中=黄,高=红(音乐播放器常见配色)
        QColor color;
        if(level < 40)      color = QColor(46, 204, 113);//绿
        else if(level < 75) color = QColor(241, 196, 15);//黄
        else                color = QColor(231, 76, 60);//红

        p.fillRect(x, y, barWidth, barH, color);
    }
}

//============================================================
// AudioChatWindow:音频通话弹窗
//============================================================
AudioChatWindow::AudioChatWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("音频通话");
    resize(360, 250);
    //(1)禁止 resize:弹窗是"小音乐播放器"风格,固定大小即可
    setFixedSize(360, 250);
    //(2)关闭按钮仅隐藏(主流程要求显式点"结束通话"才走挂断逻辑)
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    //--- 顶部:对端名字 ---
    peerNameLabel = new QLabel("正在与对方音频通话...", this);
    peerNameLabel->setAlignment(Qt::AlignCenter);
    QFont f = peerNameLabel->font();
    f.setPointSize(11);
    f.setBold(true);
    peerNameLabel->setFont(f);
    mainLayout->addWidget(peerNameLabel);

    //--- 顶部副标题:通话时长 ---
    durationLabel = new QLabel("00:00", this);
    durationLabel->setAlignment(Qt::AlignCenter);
    QFont df = durationLabel->font();
    df.setPointSize(10);
    durationLabel->setFont(df);
    durationLabel->setStyleSheet("color: #888;");
    mainLayout->addWidget(durationLabel);

    //--- 中部:对端说话强度曲线 ---
    remoteLevelBar = new AudioLevelBar(this);
    QHBoxLayout* remoteBarRow = new QHBoxLayout;
    remoteBarRow->setSpacing(6);
    QLabel* remoteHint = new QLabel("对端声音", this);
    remoteHint->setStyleSheet("color: #aaa; font-size: 9pt;");
    remoteHint->setMinimumWidth(60);
    remoteBarRow->addWidget(remoteHint);
    remoteBarRow->addWidget(remoteLevelBar, 1);
    mainLayout->addLayout(remoteBarRow);

    //--- 中部:本端说话强度曲线 ---
    localLevelBar = new AudioLevelBar(this);
    QHBoxLayout* localBarRow = new QHBoxLayout;
    localBarRow->setSpacing(6);
    QLabel* localHint = new QLabel("本端声音", this);
    localHint->setStyleSheet("color: #aaa; font-size: 9pt;");
    localHint->setMinimumWidth(60);
    localBarRow->addWidget(localHint);
    localBarRow->addWidget(localLevelBar, 1);
    mainLayout->addLayout(localBarRow);

    //--- 底部:静音 / 挂断按钮 ---
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    muteButton = new QPushButton("静音", this);
    muteButton->setCheckable(true);
    muteButton->setMinimumHeight(34);
    muteButton->setStyleSheet(
        "QPushButton { background-color: #34495e; color: white; font-size: 11pt; "
        "border: none; padding: 6px; border-radius: 4px; }"
        "QPushButton:checked { background-color: #e67e22; }");
    connect(muteButton, &QPushButton::toggled, this, &AudioChatWindow::onMuteButtonClicked);
    btnRow->addWidget(muteButton);

    hangUpButton = new QPushButton("结束通话", this);
    hangUpButton->setMinimumHeight(34);
    hangUpButton->setStyleSheet(
        "QPushButton { background-color: #e74c3c; color: white; font-size: 11pt; "
        "font-weight: bold; border: none; padding: 6px; border-radius: 4px; }");
    connect(hangUpButton, &QPushButton::clicked, this, &AudioChatWindow::hangUpClicked);
    btnRow->addWidget(hangUpButton);

    mainLayout->addLayout(btnRow);

    setLayout(mainLayout);

    //--- duration 刷新 timer(1Hz,放在主线程即可) ---
    durationTimer = new QTimer(this);
    durationTimer->setInterval(1000);
    connect(durationTimer, &QTimer::timeout, this, &AudioChatWindow::onDurationTick);
    elapsed.invalidate();//等待 startDurationTimer 显式启动
}

AudioChatWindow::~AudioChatWindow()
{
}

void AudioChatWindow::startDurationTimer()
{
    //(1)重置 elapsed 并启动 QTimer,后续每秒刷新一次 durationLabel
    //(2)注意:这里只刷新"本端弹窗的时长",对端不要求同步
    elapsed.start();
    durationLabel->setText("00:00");
    if(!durationTimer->isActive())
        durationTimer->start();
}

void AudioChatWindow::setPeerName(const QString& name)
{
    peerNameLabel->setText(QString("与 %1 音频通话中").arg(name));
}

void AudioChatWindow::pushRemoteLevel(int level)
{
    if(remoteLevelBar)
        remoteLevelBar->pushLevel(level);
}

void AudioChatWindow::pushLocalLevel(int level)
{
    if(localLevelBar)
        localLevelBar->pushLevel(level);
}

void AudioChatWindow::setMuteState(bool muted)
{
    //(1)外部主动同步(例如 onStartAudioChat 弹窗刚弹出时,默认未静音)
    //(2)用 blockSignals 避免触发 onMuteButtonClicked 形成反向递归
    if(muteButton->isChecked() == muted)
        return;
    muteButton->blockSignals(true);
    muteButton->setChecked(muted);
    muteButton->setText(muted ? "已静音" : "静音");
    muteButton->blockSignals(false);
    m_muted = muted;
}

void AudioChatWindow::onDurationTick()
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

void AudioChatWindow::onMuteButtonClicked()
{
    m_muted = muteButton->isChecked();
    //(1)更新按钮文案,直观反映"当前是否在静音"
    muteButton->setText(m_muted ? "已静音" : "静音");
    //(2)发出信号,由 mainwindow 调 audiocapture::setMuted
    emit muteToggled(m_muted);
}
