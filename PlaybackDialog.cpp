#include "PlaybackDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

PlaybackDialog::PlaybackDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New Playback");
    setMinimumWidth(360);

    m_channel = new QSpinBox;
    m_channel->setRange(1, 64);
    m_channel->setValue(1);

    QDateTime now = QDateTime::currentDateTime();

    m_startTime = new QDateTimeEdit(now);
    m_startTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startTime->setCalendarPopup(true);

    m_endTime = new QDateTimeEdit(now.addSecs(3600));
    m_endTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endTime->setCalendarPopup(true);

    auto* form = new QFormLayout;
    form->addRow("Channel:", m_channel);
    form->addRow("Start Time:", m_startTime);
    form->addRow("End Time:", m_endTime);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int PlaybackDialog::channel() const {
    return m_channel->value();
}

NET_DVR_TIME PlaybackDialog::startTime() const {
    return toSDKTime(m_startTime->dateTime());
}

NET_DVR_TIME PlaybackDialog::endTime() const {
    return toSDKTime(m_endTime->dateTime());
}

NET_DVR_TIME PlaybackDialog::toSDKTime(const QDateTime& dt) {
    NET_DVR_TIME t{};
    t.dwYear   = dt.date().year();
    t.dwMonth  = dt.date().month();
    t.dwDay    = dt.date().day();
    t.dwHour   = dt.time().hour();
    t.dwMinute = dt.time().minute();
    t.dwSecond = dt.time().second();
    return t;
}
