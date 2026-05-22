#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include "HCNetSDK.h"
#include "LoginInfo.h"

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    LONG userId() const { return m_userId; }

private slots:
    void onLoginClicked();
    void onLoginResult(LONG uid, int errCode);

private:
    QLineEdit*   m_ip;
    QSpinBox*    m_port;
    QLineEdit*   m_user;
    QLineEdit*   m_pass;
    QLabel*      m_statusLabel;
    QPushButton* m_loginBtn;
    QPushButton* m_cancelBtn;

    LONG m_userId = -1;

    void setInputsEnabled(bool enabled);
    LoginInfo currentLoginInfo() const;
};
