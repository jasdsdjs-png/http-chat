#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include<QRegularExpression>
#include<QRegularExpressionMatch>
#include"httpmgr.h"
#include"clickedlabel.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    //设置输入模式为密码模型（他人不可见）
    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->confirm_edit->setEchoMode(QLineEdit::Password);
    //设置error_label的属性，让qt去qss找属性定义
    ui->error_label->setProperty("state","normal");
    ui->error_label->setText("");
    repolish(ui->error_label);

    // 输入框失去焦点时进行验证
    //用户按回车或输入框失去焦点（点击别处/Tab 切换）时触发
    connect(ui->user_edit, &QLineEdit::editingFinished, this, &RegisterDialog::checkUserValid);
    connect(ui->email_edit, &QLineEdit::editingFinished, this, &RegisterDialog::checkEmailValid);
    connect(ui->pass_edit, &QLineEdit::editingFinished, this, &RegisterDialog::checkPassValid);
    connect(ui->confirm_edit, &QLineEdit::editingFinished, this, &RegisterDialog::checkConfirmValid);
    connect(ui->varify_edit, &QLineEdit::editingFinished, this, &RegisterDialog::checkVarifyValid);

    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reg_mod_finish,this,
            &RegisterDialog::slot_reg_mod_finish);
    initHttpHandlers();

    _countdownTimer = new QTimer(this);
    connect(_countdownTimer, &QTimer::timeout, this, &RegisterDialog::onCountdownTick);

    // 显示和隐藏密码
    // 设置光标为手型
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);

    // 初始显示闭眼图（密码隐藏状态）
    ui->pass_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
    ui->pass_visible->setScaledContents(true);
    ui->confirm_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
    ui->confirm_visible->setScaledContents(true);

    // 连接点击事件，切换图片和密码显示
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        auto state = ui->pass_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->pass_visible->setPixmap(QPixmap(":/icon/dvisible.ico"));
            ui->pass_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->pass_visible->setPixmap(QPixmap(":/icon/visible.ico"));
            ui->pass_edit->setEchoMode(QLineEdit::Normal);
        }
        qDebug() << "Label was clicked!";
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
        qDebug() << "Label was clicked!";
    });
    // 注册成功跳转登录的定时器
    _countdown_timer = new QTimer(this);
    connect(_countdown_timer, &QTimer::timeout, [this](){
        _tip_countdown--;
        if(_tip_countdown <= 0){
            _countdown_timer->stop();
            emit sigSwitchLogin();
            return;
        }
        auto str = QString("注册成功，%1 s后返回登录").arg(_tip_countdown);
        ui->tip_label->setText(str);
    });
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::onCountdownTick()
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

void RegisterDialog::on_get_btn_clicked()
{
    auto email=ui->email_edit->text().trimmed();
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    bool match=regex.match(email).hasMatch();
    if(match){
        //发送验证码
        QJsonObject json;
        json["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(
            QUrl(gate_url_prefix+"/get_varifycode"),
            json,
            ReqId::ID_GET_VARIFY_CODE,
            Modules::REGISTERMOD
            );
        //按钮置灰，等待服务器响应后再启动倒计时
        ui->get_btn->setEnabled(false);
        ui->get_btn->setText(tr("发送中..."));
    }
    else{
        ShowTip("邮箱输入错误",false);
    }
}


void RegisterDialog::ShowTip(QString str,bool is_ok){
    if(is_ok)
        ui->error_label->setProperty("state","normal");
    else ui->error_label->setProperty("state","err");
    ui->error_label->setText(str);
    repolish(ui->error_label);

}


void RegisterDialog::slot_reg_mod_finish(ReqId id,QString res,ErrorCodes err){
    if(err!=ErrorCodes::SUCCESS){
        ShowTip(tr("网络请求错误"),false);
        return;
    }
    //解析json字符串，把res通过toUtf8()转化为字节流
    QJsonDocument jsonDoc=QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()){
        ShowTip(tr("json解析失败"),false);
        return;
    }
    //判断是否是json对象
    if(!jsonDoc.isObject()){
        ShowTip(tr("json解析失败"),false);
        return;
    }

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        it.value()(jsonDoc.object());
    } else {
        qDebug() << "no handler for req id:" << id;
        return;
    }
}


void RegisterDialog::initHttpHandlers(){
    //注册验证码回包
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE,[this](const QJsonObject & jsonObj){
        int error=jsonObj["error"].toInt();
        if(error!=ErrorCodes::SUCCESS){
            if(error == ErrorCodes::RPCFailed){
                ShowTip(tr("验证码服务调用失败"),false);
            }else{
                ShowTip(tr("请求失败，错误码: %1").arg(error),false);
            }
            //恢复按钮
            ui->get_btn->setEnabled(true);
            ui->get_btn->setText(tr("获取验证码"));
            return;
        }
        auto email=jsonObj["email"].toString();
        ShowTip(tr("验证码发送到邮箱，注意查收！"),true);
        qDebug()<<"email is"<<email;
        //服务器成功返回后启动倒计时
        int ttl = jsonObj["ttl"].toInt();
        if (ttl <= 0) ttl = 300;
        _countdown = ttl;
        ui->get_btn->setEnabled(false);
        _countdownTimer->start(1000);
        //立即刷新一次显示
        int min = _countdown / 60;
        int sec = _countdown % 60;
        ui->get_btn->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
    });

    //注册注册用户回包逻辑
    _handlers.insert(ReqId::ID_REG_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            if(error == ErrorCodes::VarifyExpired){
                ShowTip(tr("验证码已过期，请重新获取"),false);
            }else if(error == ErrorCodes::VarifyCodeErr){
                ShowTip(tr("验证码错误"),false);
            }else if(error == ErrorCodes::UserExist){
                ShowTip(tr("用户或邮箱已存在"),false);
            }else if(error == ErrorCodes::PasswdErr){
                ShowTip(tr("两次密码不一致"),false);
            }else if(error == ErrorCodes::Error_Json){
                ShowTip(tr("数据格式错误"),false);
            }else{
                ShowTip(tr("注册失败，错误码: %1").arg(error),false);
            }
            return;
        }
        auto email = jsonObj["email"].toString();
        ShowTip(tr("用户注册成功"), true);
        ChangeTipPage();//切换界面
        qDebug()<< "email is " << email ;
    });
}

void RegisterDialog::checkUserValid()
{
    auto user = ui->user_edit->text().trimmed();
    if (user.isEmpty()) {
        ShowTip(tr("用户名不能为空"), false);
    } else {
         ShowTip("", true);
    }
}

void RegisterDialog::checkEmailValid()
{
    auto email = ui->email_edit->text().trimmed();
    if (email.isEmpty()) {
        ShowTip(tr("邮箱不能为空"), false);
        return;
    }
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    if (!regex.match(email).hasMatch()) {
        ShowTip(tr("邮箱格式错误"), false);
    } else {
        ShowTip("", true);
    }
}

void RegisterDialog::checkPassValid()
{
    auto pass = ui->pass_edit->text();
    if (pass.isEmpty()) {
        ShowTip(tr("密码不能为空"), false);
    } else if (pass.length() < 6) {
        ShowTip(tr("密码长度不能少于6位"), false);
    } else {
         ShowTip("", true);
    }
}

void RegisterDialog::checkConfirmValid()
{
    auto confirm = ui->confirm_edit->text();
    if (confirm.isEmpty()) {
        ShowTip(tr("确认密码不能为空"), false);
    } else if (confirm != ui->pass_edit->text()) {
        ShowTip(tr("两次输入的密码不一致"), false);
    } else {
        ShowTip("", true);
    }
}

void RegisterDialog::checkVarifyValid()
{
    auto varify = ui->varify_edit->text().trimmed();
    if (varify.isEmpty()) {
        ShowTip(tr("验证码不能为空"), false);
    } else if (varify.length() != 4) {
        ShowTip(tr("验证码长度应为4位"), false);
    } else {
        ShowTip("", true);
    }
}

void RegisterDialog::on_confirm_btn_clicked()
{
    // 依次校验所有字段，任一不通过则终止
    auto user = ui->user_edit->text().trimmed();
    if (user.isEmpty()) {
        ShowTip(tr("用户名不能为空"), false);
        return;
    }

    auto email = ui->email_edit->text().trimmed();
    if (email.isEmpty()) {
        ShowTip(tr("邮箱不能为空"), false);
        return;
    }
    QRegularExpression regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9][a-zA-Z0-9.-]*\.[a-zA-Z]{2,})");
    if (!regex.match(email).hasMatch()) {
        ShowTip(tr("邮箱格式错误"), false);
        return;
    }

    auto pass = ui->pass_edit->text();
    if (pass.isEmpty()) {
        ShowTip(tr("密码不能为空"), false);
        return;
    }
    if (pass.length() < 6) {
        ShowTip(tr("密码长度不能少于6位"), false);
        return;
    }

    auto confirm = ui->confirm_edit->text();
    if (confirm.isEmpty()) {
        ShowTip(tr("确认密码不能为空"), false);
        return;
    }
    if (confirm != pass) {
        ShowTip(tr("两次输入的密码不一致"), false);
        return;
    }

    auto varify = ui->varify_edit->text().trimmed();
    if (varify.isEmpty()) {
        ShowTip(tr("验证码不能为空"), false);
        return;
    }

    // 所有校验通过，发送请求
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["email"] = email;
    json_obj["passwd"] = pass;
    json_obj["confirm"] = confirm;
    json_obj["varifycode"] = varify;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                                           json_obj, ReqId::ID_REG_USER, Modules::REGISTERMOD);
}

// 注册成功，切换到提示页面
void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();

    ui->tip_label->setText(QString("注册成功，%1 s后返回登录").arg(_tip_countdown));
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}
void RegisterDialog::on_backlogin_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}


void RegisterDialog::on_cancel_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

