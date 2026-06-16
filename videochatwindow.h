#ifndef VIDEOCHATWINDOW_H
#define VIDEOCHATWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

class VideoChatWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VideoChatWindow(QWidget* parent = nullptr);
    ~VideoChatWindow();

    void showLocalVideo(const QImage& frame);
    void showRemoteVideo(const QImage& frame, int peerHostNum);
    void setPeerName(const QString& name);

    //(1)启动内部的"通话时长"QTimer,从当前时刻开始累计
    //   - 由 mainwindow 在 confirmStartVideoChat(创建弹窗后)调用
    //   - 这样 duration 从"0"开始,与 audio 弹窗一样
    void startDurationTimer();
    //(1)把外部的"静音"状态同步到按钮上(初始化时调用,默认非静音)
    void setMuteState(bool muted);

signals:
    void hangUpClicked();
    //(1)用户切换了"静音"按钮 -> 由 mainwindow 转发到 audiocapture::setMuted
    void muteToggled(bool muted);

private slots:
    void onDurationTick();
    void onMuteButtonClicked();

private:
    QLabel* localVideoLabel;
    QLabel* remoteVideoLabel;
    QLabel* peerNameLabel;
    QLabel* durationLabel;//新增:显示 00:xx / xx:xx
    QPushButton* muteButton;//新增:静音 toggle
    QPushButton* hangUpButton;
    QTimer* durationTimer;
    QElapsedTimer elapsed;
    bool m_muted{false};
};

#endif // VIDEOCHATWINDOW_H
