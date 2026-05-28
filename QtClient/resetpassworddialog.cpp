#include "resetpassworddialog.h"
#include "ui_resetpassworddialog.h"
#include "global.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "httpmgr.h"
#include "clickedlabel.h"

// 构造函数：初始化UI、密码显示模式、信号连接、定时器
ResetPasswordDialog::ResetPasswordDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetPasswordDialog)
{
    ui->setupUi(this);
    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->confirm_edit->setEchoMode(QLineEdit::Password);

    ui->error_label->setProperty("state", "normal");
    ui->error_label->setText("");
    repolish(ui->error_label);

    // 输入框失焦校验
    connect(ui->user_edit, &QLineEdit::editingFinished, this, &ResetPasswordDialog::checkUserValid);
    connect(ui->email_edit, &QLineEdit::editingFinished, this, &ResetPasswordDialog::checkEmailValid);
    connect(ui->pass_edit, &QLineEdit::editingFinished, this, &ResetPasswordDialog::checkPassValid);
    connect(ui->confirm_edit, &QLineEdit::editingFinished, this, &ResetPasswordDialog::checkConfirmValid);
    connect(ui->varify_edit, &QLineEdit::editingFinished, this, &ResetPasswordDialog::checkVarifyValid);

    // 绑定HTTP响应信号
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_reset_mod_finish, this,
            &ResetPasswordDialog::slot_reset_mod_finish);
    initHttpHandlers();

    // 验证码倒计时定时器
    _countdownTimer = new QTimer(this);
    connect(_countdownTimer, &QTimer::timeout, this, &ResetPasswordDialog::onCountdownTick);

    // 密码可见性切换按钮初始化
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
    ui->pass_visible->setScaledContents(true);
    ui->confirm_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
    ui->confirm_visible->setScaledContents(true);

    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        auto state = ui->pass_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->pass_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
            ui->pass_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->pass_visible->setPixmap(QPixmap(":/icon/visible.ico"));
            ui->pass_edit->setEchoMode(QLineEdit::Normal);
        }
    });

    connect(ui->confirm_visible, &ClickedLabel::clicked, this, [this]() {
        auto state = ui->confirm_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->confirm_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
            ui->confirm_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->confirm_visible->setPixmap(QPixmap(":/icon/visible.ico"));
            ui->confirm_edit->setEchoMode(QLineEdit::Normal);
        }
    });

    // 重置成功后自动返回登录的倒计时
    _countdown_timer = new QTimer(this);
    connect(_countdown_timer, &QTimer::timeout, [this](){
        _tip_countdown--;
        if(_tip_countdown <= 0){
            _countdown_timer->stop();
            emit sigSwitchLogin();
            return;
        }
        auto str = QString("密码重置成功，%1 s后返回登录").arg(_tip_countdown);
        ui->tip_label->setText(str);
    });
}

ResetPasswordDialog::~ResetPasswordDialog()
{
    delete ui;
}

// 验证码按钮倒计时，每秒更新按钮文字
void ResetPasswordDialog::onCountdownTick()
{
    _countdown--;
    if(_countdown <= 0){
        _countdownTimer->stop();
        ui->get_btn->setEnabled(true);
        ui->get_btn->setText(tr("获取验证码"));
    }else{
        int min = _countdown / 60;
        int sec = _countdown % 60;
        ui->get_btn->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
    }
}

// 点击获取验证码：校验用户名和邮箱，发送HTTP请求
void ResetPasswordDialog::on_get_btn_clicked()
{
    auto user = ui->user_edit->text().trimmed();
    if(user.isEmpty()){
        ShowTip(tr("用户名不能为空"), false);
        return;
    }

    auto email = ui->email_edit->text().trimmed();
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    bool match = regex.match(email).hasMatch();
    if(match){
        QJsonObject json;
        json["user"] = user;
        json["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(
            QUrl(gate_url_prefix + "/get_varifycode"),
            json,
            ReqId::ID_GET_VARIFY_CODE,
            Modules::RESETPWDMOD
        );
        ui->get_btn->setEnabled(false);
        ui->get_btn->setText(tr("发送中..."));
    } else {
        ShowTip("邮箱输入错误", false);
    }
}

// 显示提示信息，通过qss切换正常/错误样式
void ResetPasswordDialog::ShowTip(QString str, bool is_ok){
    if(is_ok)
        ui->error_label->setProperty("state", "normal");
    else
        ui->error_label->setProperty("state", "err");
    ui->error_label->setText(str);
    repolish(ui->error_label);
}

// HTTP响应统一入口：解析JSON并分发到对应handler
void ResetPasswordDialog::slot_reset_mod_finish(ReqId id, QString res, ErrorCodes err){
    if(err != ErrorCodes::SUCCESS){
        ShowTip(tr("网络请求错误"), false);
        return;
    }
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()){
        ShowTip(tr("json解析失败"), false);
        return;
    }
    if(!jsonDoc.isObject()){
        ShowTip(tr("json解析失败"), false);
        return;
    }

    auto it = _handlers.find(id);
    if(it != _handlers.end()){
        it.value()(jsonDoc.object());
    } else {
        qDebug() << "no handler for req id:" << id;
        return;
    }
}

// 注册HTTP响应handler：验证码发送结果、密码重置结果
void ResetPasswordDialog::initHttpHandlers(){
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE, [this](const QJsonObject & jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            if(error == ErrorCodes::RPCFailed){
                ShowTip(tr("验证码服务调用失败"), false);
            } else {
                ShowTip(tr("请求失败，错误码: %1").arg(error), false);
            }
            ui->get_btn->setEnabled(true);
            ui->get_btn->setText(tr("获取验证码"));
            return;
        }
        auto email = jsonObj["email"].toString();
        ShowTip(tr("验证码发送到邮箱，注意查收！"), true);
        qDebug() << "email is" << email;
        int ttl = jsonObj["ttl"].toInt();
        if(ttl <= 0) ttl = 300;
        _countdown = ttl;
        ui->get_btn->setEnabled(false);
        _countdownTimer->start(1000);
        int min = _countdown / 60;
        int sec = _countdown % 60;
        ui->get_btn->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
    });

    _handlers.insert(ReqId::ID_RESET_PWD, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            if(error == ErrorCodes::VarifyExpired){
                ShowTip(tr("验证码已过期，请重新获取"), false);
            } else if(error == ErrorCodes::VarifyCodeErr){
                ShowTip(tr("验证码错误"), false);
            } else if(error == ErrorCodes::UserNotExist){
                ShowTip(tr("用户名不存在"), false);
            } else if(error == ErrorCodes::EmailNotMatch){
                ShowTip(tr("邮箱不存在"), false);
            } else if(error == ErrorCodes::PasswdErr){
                ShowTip(tr("两次密码不一致"), false);
            } else if(error == ErrorCodes::PasswdInvalid){
                ShowTip(tr("密码格式无效"), false);
            } else if(error == ErrorCodes::Error_Json){
                ShowTip(tr("数据格式错误"), false);
            } else {
                ShowTip(tr("密码重置失败，错误码: %1").arg(error), false);
            }
            return;
        }
        ShowTip(tr("密码重置成功"), true);
        ChangeTipPage();
    });
}

// 校验用户名非空
void ResetPasswordDialog::checkUserValid()
{
    auto user = ui->user_edit->text().trimmed();
    if(user.isEmpty()){
        ShowTip(tr("用户名不能为空"), false);
    } else {
        ShowTip("", true);
    }
}

// 校验邮箱格式
void ResetPasswordDialog::checkEmailValid()
{
    auto email = ui->email_edit->text().trimmed();
    if(email.isEmpty()){
        ShowTip(tr("邮箱不能为空"), false);
        return;
    }
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    if(!regex.match(email).hasMatch()){
        ShowTip(tr("邮箱格式错误"), false);
    } else {
        ShowTip("", true);
    }
}

// 校验密码长度不少于6位
void ResetPasswordDialog::checkPassValid()
{
    auto pass = ui->pass_edit->text();
    if(pass.isEmpty()){
        ShowTip(tr("密码不能为空"), false);
    } else if(pass.length() < 6){
        ShowTip(tr("密码长度不能少于6位"), false);
    } else {
        ShowTip("", true);
    }
}

// 校验两次密码是否一致
void ResetPasswordDialog::checkConfirmValid()
{
    auto confirm = ui->confirm_edit->text();
    if(confirm.isEmpty()){
        ShowTip(tr("确认密码不能为空"), false);
    } else if(confirm != ui->pass_edit->text()){
        ShowTip(tr("两次输入的密码不一致"), false);
    } else {
        ShowTip("", true);
    }
}

// 校验验证码长度为4位
void ResetPasswordDialog::checkVarifyValid()
{
    auto varify = ui->varify_edit->text().trimmed();
    if(varify.isEmpty()){
        ShowTip(tr("验证码不能为空"), false);
    } else if(varify.length() != 4){
        ShowTip(tr("验证码长度应为4位"), false);
    } else {
        ShowTip("", true);
    }
}

// 点击确认：依次校验所有字段，通过后发送重置密码请求
void ResetPasswordDialog::on_confirm_btn_clicked()
{
    auto user = ui->user_edit->text().trimmed();
    if(user.isEmpty()){
        ShowTip(tr("用户名不能为空"), false);
        return;
    }

    auto email = ui->email_edit->text().trimmed();
    if(email.isEmpty()){
        ShowTip(tr("邮箱不能为空"), false);
        return;
    }
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    if(!regex.match(email).hasMatch()){
        ShowTip(tr("邮箱格式错误"), false);
        return;
    }

    auto pass = ui->pass_edit->text();
    if(pass.isEmpty()){
        ShowTip(tr("密码不能为空"), false);
        return;
    }
    if(pass.length() < 6){
        ShowTip(tr("密码长度不能少于6位"), false);
        return;
    }

    auto confirm = ui->confirm_edit->text();
    if(confirm.isEmpty()){
        ShowTip(tr("确认密码不能为空"), false);
        return;
    }
    if(confirm != pass){
        ShowTip(tr("两次输入的密码不一致"), false);
        return;
    }

    auto varify = ui->varify_edit->text().trimmed();
    if(varify.isEmpty()){
        ShowTip(tr("验证码不能为空"), false);
        return;
    }

    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["email"] = email;
    json_obj["passwd"] = pass;
    json_obj["confirm"] = confirm;
    json_obj["varifycode"] = varify;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/reset_pwd"),
                                         json_obj, ReqId::ID_RESET_PWD, Modules::RESETPWDMOD);
}

// 切换到成功提示页，启动自动返回登录的倒计时
void ResetPasswordDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    ui->tip_label->setText(QString("密码重置成功，%1 s后返回登录").arg(_tip_countdown));
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

// 点击返回登录：停止定时器并发出切回登录信号
void ResetPasswordDialog::on_backlogin_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

// 点击取消：停止定时器并发出切回登录信号
void ResetPasswordDialog::on_cancel_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}
