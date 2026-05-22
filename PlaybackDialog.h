#pragma once
#include <QDateTimeEdit>
#include <QDialog>
#include <QSpinBox>
#include "HCNetSDK.h"

class PlaybackDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlaybackDialog(QWidget* parent = nullptr);

    int          channel()   const;
    NET_DVR_TIME startTime() const;
    NET_DVR_TIME endTime()   const;

private:
    QSpinBox*      m_channel;
    QDateTimeEdit* m_startTime;
    QDateTimeEdit* m_endTime;

    static NET_DVR_TIME toSDKTime(const QDateTime& dt);
};
