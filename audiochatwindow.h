#ifndef AUDIOCHATWINDOW_H
#define AUDIOCHATWINDOW_H

//音频通话弹窗(类似"音乐播放器"的小窗,无视频画面)
//(1)显示对端名字 + 通话已持续时长
//(2)显示对端说话强度曲线(从 audiodecoder::sendDecodedAudioLevel 喂入)
//(3)提供"静音"按钮(toggle),向 mainwindow 发 muteToggled 信号
//(4)提供"结束通话"按钮,向 mainwindow 发 hangUpClicked 信号
//(5)弹窗的"通话时长"使用内部 QTimer 周期刷新,主线程安全
//   (弹窗本身就在主线程,不需要跨线程)

#include <QDialog>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QVector>

class QPaintEvent;

//--- 自绘的"对端说话强度"曲线组件 ---
//(1)内部维护一个固定长度的循环缓冲(默认 60 个槽位 ≈ 1.2s @ 50fps/20ms帧)
//(2)每次 pushLevel 新值推进一格,旧的从尾部滑出
//(3)在 paintEvent 里从左到右画 60 根细竖条,顶端高度正比于 level(0~100)
//(4)纯自绘,不依赖 QChart/第三方库,体积小
class AudioLevelBar : public QWidget
{
    Q_OBJECT
public:
    explicit AudioLevelBar(QWidget* parent = nullptr);
    //(1)推入一帧新的 level(0~100),会自动触发重绘
    //(2)线程安全:在主线程调用(paintEvent 在主线程)
    void pushLevel(int level);
    //重置曲线(关闭/开启弹窗时清空旧值)
    void resetLevels();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kBarCount = 60;//曲线宽度(根数)
    QVector<int> m_levels;//长度=kBarCount,环形缓冲
    int m_head{0};//下一个写入位置
};

//--- 音频通话主弹窗 ---
class AudioChatWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AudioChatWindow(QWidget* parent = nullptr);
    ~AudioChatWindow();

    //(1)重置内部计时器,从当前时刻开始累计 duration
    //   - 由 mainwindow 在 accept 后(或者自己发起并已建立 pipeline 后)调用
    //   - 这样 duration 的"0"就是会话真正开始的时间
    void startDurationTimer();
    //(1)把对端名字写到标题栏 + 顶部标签
    void setPeerName(const QString& name);
    //(1)从 mainwindow 注入对端音量(主线程)
    void pushRemoteLevel(int level);
    //(1)从 mainwindow 注入本端音量(主线程,静音时为 0)
    void pushLocalLevel(int level);
    //(1)把弹窗上的"静音"按钮状态同步到外部(例如对方 accept 后,弹窗自动解除静音)
    void setMuteState(bool muted);

signals:
    //(1)用户点击了"结束通话"按钮
    void hangUpClicked();
    //(1)用户切换了"静音"按钮
    //   - muted=true:本端暂停 audiocapture 的 floodTimer,停止向对端送 PCM
    //   - muted=false:恢复
    void muteToggled(bool muted);

private slots:
    //(1)QTimer 周期回调:刷新顶部的"通话时长"标签
    void onDurationTick();
    //(1)静音按钮 clicked 槽
    void onMuteButtonClicked();

private:
    QLabel*     peerNameLabel;
    QLabel*     durationLabel;
    AudioLevelBar* remoteLevelBar;//对端说话强度
    AudioLevelBar* localLevelBar;//本端说话强度
    QPushButton* muteButton;
    QPushButton* hangUpButton;
    QTimer*     durationTimer;
    QElapsedTimer elapsed;
    bool        m_muted{false};
};

#endif // AUDIOCHATWINDOW_H
