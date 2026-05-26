#include "MainWindow.h"
#include "DashServer.h"
#include "PlaybackDialog.h"
#include "PlaybackManagerDialog.h"
#include "StreamJobManager.h"
#include "VideoKey.h"
#include "VideoPlayerWidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    const QString downloadsDir = QCoreApplication::applicationDirPath() + "/downloads";
    QDir().mkpath(downloadsDir);

    m_dashServer = new DashServer(downloadsDir, 8080, this);
    if (!m_dashServer->start()) {
        QMessageBox::critical(nullptr, "Server Error",
                              "Could not bind DASH server on port 8080.\n"
                              "Another process may already be using that port.");
    }

    m_jobManager = new StreamJobManager(downloadsDir, this);
    connect(m_jobManager, &StreamJobManager::streamReady,
            this, &MainWindow::onStreamReady);
    connect(m_jobManager, &StreamJobManager::streamError,
            this, &MainWindow::onStreamError);
    connect(m_jobManager, &StreamJobManager::downloadProgress,
            this, &MainWindow::onDownloadProgress);

    connect(m_dashServer, &DashServer::streamRequested,
            this, &MainWindow::onStreamRequested);

    setupUi();
}

MainWindow::~MainWindow() {
    if (m_userId >= 0) {
        NET_DVR_Logout_V30(m_userId);
        m_userId = -1;
    }
}

void MainWindow::setUserId(LONG userId) {
    m_userId = userId;
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    m_newPlaybackBtn = new QPushButton("New Playback", this);
    m_newPlaybackBtn->setFixedHeight(36);

    m_managePlaybacksBtn = new QPushButton("Manage Playbacks", this);
    m_managePlaybacksBtn->setFixedHeight(36);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_newPlaybackBtn);
    toolbar->addWidget(m_managePlaybacksBtn);
    toolbar->addStretch();

    m_statusLabel = new QLabel("Ready.", this);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->hide();

    m_playerWidget = new VideoPlayerWidget(this);
    m_playerWidget->hide();

    auto* layout = new QVBoxLayout(central);
    layout->addLayout(toolbar);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_playerWidget, 1);

    connect(m_newPlaybackBtn,     &QPushButton::clicked, this, &MainWindow::onNewPlaybackClicked);
    connect(m_managePlaybacksBtn, &QPushButton::clicked, this, &MainWindow::onManagePlaybacksClicked);
}

void MainWindow::onManagePlaybacksClicked() {
    const QString downloadsPath = QCoreApplication::applicationDirPath() + "/downloads";
    PlaybackManagerDialog dlg(downloadsPath, this);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedPath().isEmpty())
        return;

    const QString path = dlg.selectedPath();
    m_progressBar->hide();
    m_statusLabel->setText("Playing: " + QFileInfo(path).fileName());
    m_playerWidget->show();
    m_playerWidget->play(path);
}

void MainWindow::onStreamRequested(int channel, const QString& startTime, const QString& endTime) {
    if (m_userId < 0)
        return; // not logged in; can't download

    const NET_DVR_TIME s = VideoKey::parseTime(startTime);
    const NET_DVR_TIME e = VideoKey::parseTime(endTime);

    m_statusLabel->setText(QString("Preparing stream for ch%1…").arg(channel));
    m_progressBar->setValue(0);
    m_progressBar->show();

    m_jobManager->requestStream(m_userId, channel, s, e);
}

void MainWindow::onNewPlaybackClicked() {
    PlaybackDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_newPlaybackBtn->setEnabled(false);
    m_statusLabel->setText("Preparing stream…");
    m_progressBar->setValue(0);
    m_progressBar->show();
    m_playerWidget->hide();

    m_jobManager->requestStream(m_userId, dlg.channel(), dlg.startTime(), dlg.endTime());
}

void MainWindow::onStreamReady(const QString& mpdUrl) {
    m_progressBar->hide();
    m_newPlaybackBtn->setEnabled(true);
    m_statusLabel->setText("Streaming: " + mpdUrl);
    m_playerWidget->show();
    m_playerWidget->play(mpdUrl);
}

void MainWindow::onStreamError(const QString& reason) {
    m_progressBar->hide();
    m_newPlaybackBtn->setEnabled(true);
    m_statusLabel->setText("Idle.");
    QMessageBox::critical(this, "Stream Error", reason);
}

void MainWindow::onDownloadProgress(int /*channel*/, int percent) {
    m_progressBar->setValue(percent);
    m_statusLabel->setText(QString("Downloading… %1%").arg(percent));
}
