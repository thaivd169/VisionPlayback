#pragma once
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include "HCNetSDK.h"
#include "PlaybackRequest.h"

class DownloadWorker;
class VideoPlayerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void setUserId(LONG userId);

private slots:
    void onNewPlaybackClicked();
    void onManagePlaybacksClicked();
    void onProgressChanged(int percent);
    void onDownloadFinished(bool success, const QString& errorMsg);

private:
    LONG             m_userId = -1;
    QString          m_outputPath;

    QPushButton*     m_newPlaybackBtn;
    QPushButton*     m_managePlaybacksBtn;
    QLabel*          m_statusLabel;
    QProgressBar*    m_progressBar;
    VideoPlayerWidget* m_playerWidget;

    DownloadWorker*  m_worker      = nullptr;
    QThread*         m_workerThread = nullptr;

    void setupUi();
    void startWorker(const PlaybackRequest& req);
};
