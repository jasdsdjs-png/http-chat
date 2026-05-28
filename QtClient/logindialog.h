#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H
#include "global.h"

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
    bool checkUserValid();
    bool checkPwdValid();
    void showTip(QString str, bool is_ok);
    void enableBtn(bool enabled);
    void initHttpHandlers();

    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
    Ui::LoginDialog *ui;
    int _uid = 0;
    QString _token;

signals:
    void switchRegister();
    void switchForgetPassword();
    void sig_connect_tcp(ServerInfo si);

private slots:
    void on_LoginButton_clicked();
    void slot_login_mod_finish(ReqId id, QString res, ErrorCodes err);
    void slot_tcp_con_finish(bool bsuccess);
    void slot_tcp_login_failed(int err);
};

#endif // LOGINDIALOG_H
