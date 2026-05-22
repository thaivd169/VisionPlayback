#include "LoginDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <cstring>

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("VisionPlayback — Login");
    setMinimumWidth(360);

    m_ip = new QLineEdit;
    m_ip->setText("14.248.90.240");

    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(8002);

    m_user = new QLineEdit;
    m_user->setText("thai.vd");

    m_pass = new QLineEdit;
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setPlaceholderText("password");

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("color: red;");
    m_statusLabel->hide();

    m_loginBtn  = new QPushButton("Login");
    m_cancelBtn = new QPushButton("Cancel");
    m_loginBtn->setDefault(true);

    auto* form = new QFormLayout;
    form->addRow("IP Address:", m_ip);
    form->addRow("Port:", m_port);
    form->addRow("Username:", m_user);
    form->addRow("Password:", m_pass);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_statusLabel);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_loginBtn);
    layout->addLayout(btnRow);

    connect(m_loginBtn,  &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

LoginInfo LoginDialog::currentLoginInfo() const {
    LoginInfo info{};
    strncpy(info.ip,   m_ip->text().toUtf8().constData(),   sizeof(info.ip)   - 1);
    strncpy(info.user, m_user->text().toUtf8().constData(), sizeof(info.user) - 1);
    strncpy(info.pass, m_pass->text().toUtf8().constData(), sizeof(info.pass) - 1);
    info.port = static_cast<uint16_t>(m_port->value());
    return info;
}

void LoginDialog::setInputsEnabled(bool enabled) {
    m_ip->setEnabled(enabled);
    m_port->setEnabled(enabled);
    m_user->setEnabled(enabled);
    m_pass->setEnabled(enabled);
    m_loginBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(enabled);
}

void LoginDialog::onLoginClicked() {
    m_statusLabel->hide();
    setInputsEnabled(false);
    m_loginBtn->setText("Connecting…");

    LoginInfo info = currentLoginInfo();

    auto* watcher = new QFutureWatcher<QPair<LONG, int>>(this);
    connect(watcher, &QFutureWatcher<QPair<LONG, int>>::finished, this, [this, watcher]() {
        auto result = watcher->result();
        watcher->deleteLater();
        onLoginResult(result.first, result.second);
    });

    watcher->setFuture(QtConcurrent::run([info]() -> QPair<LONG, int> {
        NET_DVR_USER_LOGIN_INFO loginInfo = {};
        NET_DVR_DEVICEINFO_V40  devInfo   = {};
        loginInfo.bUseAsynLogin = false;
        loginInfo.wPort = info.port;
        memcpy(loginInfo.sDeviceAddress, info.ip,   NET_DVR_DEV_ADDRESS_MAX_LEN);
        memcpy(loginInfo.sUserName,      info.user, NAME_LEN);
        memcpy(loginInfo.sPassword,      info.pass, NAME_LEN);

        LONG uid = NET_DVR_Login_V40(&loginInfo, &devInfo);
        int  err = (uid < 0) ? NET_DVR_GetLastError() : 0;
        return {uid, err};
    }));
}

void LoginDialog::onLoginResult(LONG uid, int errCode) {
    m_loginBtn->setText("Login");
    if (uid < 0) {
        setInputsEnabled(true);
        m_statusLabel->setText(QString("Login failed (error %1)").arg(errCode));
        m_statusLabel->show();
    } else {
        m_userId = uid;
        accept();
    }
}
