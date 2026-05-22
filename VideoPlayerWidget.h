#pragma once
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSlider>
#include <QVideoWidget>
#include <QWidget>

class VideoPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    void play(const QString& filePath);

private slots:
    void onPlayPause();
    void onRewind10s();
    void onSpeedChanged(double rate);
    void onDurationChanged(qint64 ms);
    void onPositionChanged(qint64 ms);
    void onSeekBarMoved(int value);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    QMediaPlayer*  m_player;
    QVideoWidget*  m_video;
    QPushButton*   m_playPauseBtn;
    QSlider*       m_seekBar;
    QLabel*        m_timeLabel;
    QLabel*        m_errorLabel;
    QPushButton*   m_activeSpeedBtn = nullptr;

    bool m_seekBarDragging = false;

    static QString formatMs(qint64 ms);
};
