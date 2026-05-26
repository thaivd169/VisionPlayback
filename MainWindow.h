#pragma once
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include "HCNetSDK.h"

class DashServer;
class StreamJobManager;
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
    void onStreamRequested(int channel, const QString& startTime, const QString& endTime);
    void onStreamReady(const QString& mpdUrl);
    void onStreamError(const QString& reason);
    void onDownloadProgress(int channel, int percent);

private:
    LONG m_userId = -1;

    QPushButton*      m_newPlaybackBtn;
    QPushButton*      m_managePlaybacksBtn;
    QLabel*           m_statusLabel;
    QProgressBar*     m_progressBar;
    VideoPlayerWidget* m_playerWidget;

    DashServer*       m_dashServer  = nullptr;
    StreamJobManager* m_jobManager  = nullptr;

    void setupUi();
};
