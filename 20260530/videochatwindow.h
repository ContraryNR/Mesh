#ifndef VIDEOCHATWINDOW_H
#define VIDEOCHATWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>

class VideoChatWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VideoChatWindow(QWidget* parent = nullptr);
    ~VideoChatWindow();

    void showLocalVideo(const QImage& frame);
    void showRemoteVideo(const QImage& frame, int peerHostNum);
    void setPeerName(const QString& name);

signals:
    void hangUpClicked();

private:
    QLabel* localVideoLabel;
    QLabel* remoteVideoLabel;
    QLabel* peerNameLabel;
    QPushButton* hangUpButton;
};

#endif // VIDEOCHATWINDOW_H
