#include "VideoPlayerWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAudioOutput>

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent) : QWidget(parent) {
    m_player = new QMediaPlayer(this);
    auto* audio = new QAudioOutput(this);
    m_player->setAudioOutput(audio);

    m_video = new QVideoWidget(this);
    m_player->setVideoOutput(m_video);

    m_playPauseBtn = new QPushButton("▶", this);
    m_playPauseBtn->setFixedWidth(40);

    auto* rewindBtn = new QPushButton("◀ 10s", this);
    rewindBtn->setFixedWidth(55);

    m_seekBar = new QSlider(Qt::Horizontal, this);
    m_seekBar->setRange(0, 1000);

    m_timeLabel = new QLabel("0:00 / 0:00", this);
    m_timeLabel->setFixedWidth(110);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: red;");
    m_errorLabel->hide();

    // Speed buttons
    struct SpeedDef { const char* label; double rate; };
    const SpeedDef speeds[] = {{"0.5×", 0.5}, {"1×", 1.0}, {"2×", 2.0}, {"4×", 4.0}};

    auto* speedRow = new QHBoxLayout;
    speedRow->addWidget(new QLabel("Speed:"));
    for (const auto& s : speeds) {
        auto* btn = new QPushButton(s.label, this);
        btn->setFixedWidth(42);
        btn->setCheckable(true);
        if (s.rate == 1.0) {
            btn->setChecked(true);
            m_activeSpeedBtn = btn;
        }
        const double rate = s.rate;
        connect(btn, &QPushButton::clicked, this, [this, btn, rate]() {
            if (m_activeSpeedBtn) m_activeSpeedBtn->setChecked(false);
            m_activeSpeedBtn = btn;
            btn->setChecked(true);
            onSpeedChanged(rate);
        });
        speedRow->addWidget(btn);
    }
    speedRow->addStretch();

    auto* controlRow = new QHBoxLayout;
    controlRow->addWidget(rewindBtn);
    controlRow->addWidget(m_playPauseBtn);
    controlRow->addWidget(m_seekBar);
    controlRow->addWidget(m_timeLabel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_video, 1);
    layout->addWidget(m_errorLabel);
    layout->addLayout(controlRow);
    layout->addLayout(speedRow);

    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPlayerWidget::onPlayPause);
    connect(rewindBtn,      &QPushButton::clicked, this, &VideoPlayerWidget::onRewind10s);
    connect(m_seekBar, &QSlider::sliderPressed,  this, [this]() { m_seekBarDragging = true; });
    connect(m_seekBar, &QSlider::sliderReleased, this, [this]() {
        m_seekBarDragging = false;
        onSeekBarMoved(m_seekBar->value());
    });
    connect(m_player, &QMediaPlayer::durationChanged,      this, &VideoPlayerWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,      this, &VideoPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &VideoPlayerWidget::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& msg) {
        m_errorLabel->setText("Playback error: " + msg);
        m_errorLabel->show();
    });
}

void VideoPlayerWidget::play(const QString& filePath) {
    m_errorLabel->hide();
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
}

void VideoPlayerWidget::onPlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void VideoPlayerWidget::onRewind10s() {
    m_player->setPosition(qMax(0LL, m_player->position() - 10000LL));
}

void VideoPlayerWidget::onSpeedChanged(double rate) {
    m_player->setPlaybackRate(rate);
}

void VideoPlayerWidget::onDurationChanged(qint64 ms) {
    m_timeLabel->setText(formatMs(m_player->position()) + " / " + formatMs(ms));
}

void VideoPlayerWidget::onPositionChanged(qint64 ms) {
    if (!m_seekBarDragging && m_player->duration() > 0)
        m_seekBar->setValue(static_cast<int>(ms * 1000 / m_player->duration()));
    m_timeLabel->setText(formatMs(ms) + " / " + formatMs(m_player->duration()));
}

void VideoPlayerWidget::onSeekBarMoved(int value) {
    if (m_player->duration() > 0)
        m_player->setPosition(static_cast<qint64>(value) * m_player->duration() / 1000);
}

void VideoPlayerWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    m_playPauseBtn->setText(state == QMediaPlayer::PlayingState ? "⏸" : "▶");
}

QString VideoPlayerWidget::formatMs(qint64 ms) {
    int totalSec = static_cast<int>(ms / 1000);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
}
