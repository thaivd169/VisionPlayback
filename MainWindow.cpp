#include "MainWindow.h"
#include "DownloadWorker.h"
#include "PlaybackDialog.h"
#include "PlaybackManagerDialog.h"
#include "VideoPlayerWidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {
    // Stop any running download cleanly before logout
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_worker) m_worker->cancel();
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
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

    connect(m_newPlaybackBtn,      &QPushButton::clicked, this, &MainWindow::onNewPlaybackClicked);
    connect(m_managePlaybacksBtn, &QPushButton::clicked, this, &MainWindow::onManagePlaybacksClicked);
}

void MainWindow::onManagePlaybacksClicked() {
    QString downloadsPath = QCoreApplication::applicationDirPath() + "/downloads";
    PlaybackManagerDialog dlg(downloadsPath, this);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedPath().isEmpty())
        return;

    m_outputPath = dlg.selectedPath();
    m_progressBar->hide();
    m_statusLabel->setText("Playing: " + QFileInfo(m_outputPath).fileName());
    m_playerWidget->show();
    m_playerWidget->play(m_outputPath);
}

void MainWindow::onNewPlaybackClicked() {
    PlaybackDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_outputPath = QCoreApplication::applicationDirPath()
                 + "/downloads/" + ts
                 + "_ch" + QString::number(dlg.channel()) + ".mp4";
    QDir().mkpath(QFileInfo(m_outputPath).absolutePath());

    PlaybackRequest req;
    req.userId    = m_userId;
    req.channel   = dlg.channel();
    req.startTime = dlg.startTime();
    req.endTime   = dlg.endTime();
    req.outputPath = m_outputPath;

    startWorker(req);
}

void MainWindow::startWorker(const PlaybackRequest& req) {
    m_newPlaybackBtn->setEnabled(false);
    m_statusLabel->setText("Downloading…");
    m_progressBar->setValue(0);
    m_progressBar->show();
    m_playerWidget->hide();

    m_worker = new DownloadWorker(req);
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started,
            m_worker, &DownloadWorker::run);
    connect(m_worker, &DownloadWorker::progressChanged,
            this, &MainWindow::onProgressChanged);
    connect(m_worker, &DownloadWorker::finished,
            this, &MainWindow::onDownloadFinished);
    connect(m_worker, &DownloadWorker::finished,
            m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished,
            m_workerThread, &QObject::deleteLater);

    m_workerThread->start();
}

void MainWindow::onProgressChanged(int percent) {
    m_progressBar->setValue(percent);
    m_statusLabel->setText(QString("Downloading… %1%").arg(percent));
}

void MainWindow::onDownloadFinished(bool success, const QString& errorMsg) {
    m_progressBar->hide();
    m_newPlaybackBtn->setEnabled(true);

    if (!success) {
        m_statusLabel->setText("Idle.");
        QMessageBox::critical(this, "Download Failed", errorMsg);
        return;
    }

    m_statusLabel->setText("Playing: " + QFileInfo(m_outputPath).fileName());
    m_playerWidget->show();
    m_playerWidget->play(m_outputPath);
}
