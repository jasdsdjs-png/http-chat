#ifndef RESETPASSWORDDIALOG_H
#define RESETPASSWORDDIALOG_H

#include <QDialog>
#include <QTimer>
#include "global.h"

namespace Ui {
class ResetPasswordDialog;
}

class ResetPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetPasswordDialog(QWidget *parent = nullptr);
    ~ResetPasswordDialog();

private slots:
    void on_get_btn_clicked();
    void slot_reset_mod_finish(ReqId id, QString res, ErrorCodes err);
    void checkUserValid();
    void checkEmailValid();
    void checkPassValid();
    void checkConfirmValid();
    void checkVarifyValid();
    void on_confirm_btn_clicked();
    void onCountdownTick();
    void on_cancel_btn_clicked();
    void on_backlogin_btn_clicked();

signals:
    void sigSwitchLogin();

private:
    void ChangeTipPage();
    void ShowTip(QString str, bool is_ok);
    void initHttpHandlers();
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
    Ui::ResetPasswordDialog *ui;
    QTimer* _countdownTimer;
    QTimer* _countdown_timer;
    int _countdown = 0;
    int _tip_countdown = 5;
};

#endif // RESETPASSWORDDIALOG_H
