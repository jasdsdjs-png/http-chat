#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QTimer>
#include"global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    // 点击"获取验证码"按钮，校验邮箱并发送验证码请求
    void on_get_btn_clicked();
    // HTTP 请求完成后的统一回调，根据请求 ID 分发到对应处理器
    void slot_reg_mod_finish(ReqId id,QString res,ErrorCodes err);
    // 校验用户名是否为空
    void checkUserValid();
    // 校验邮箱格式
    void checkEmailValid();
    // 校验密码长度（不少于6位）
    void checkPassValid();
    // 校验两次输入的密码是否一致
    void checkConfirmValid();
    // 校验验证码长度（应为4位）
    void checkVarifyValid();

    // 点击"确认"按钮，校验所有字段后发送注册请求
    void on_confirm_btn_clicked();
    // 验证码按钮倒计时每秒触发，更新按钮显示文字
    void onCountdownTick();
    // 点击"取消"按钮，停止定时器并返回登录界面
    void on_cancel_btn_clicked();
    // 点击注册成功页面的"返回登录"按钮，停止定时器并返回登录界面
    void on_backlogin_btn_clicked();

signals:
    // 通知主窗口切换回登录界面
    void sigSwitchLogin();

private:
    void ChangeTipPage();
    void ShowTip(QString str,bool is_ok);
    void initHttpHandlers();
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    Ui::RegisterDialog *ui;
    QTimer* _countdownTimer;
    QTimer* _countdown_timer;
    int _countdown = 0;//用来接受服务器返回的ttl，即验证码过期时间
    int _tip_countdown=5;
};

#endif // REGISTERDIALOG_H
