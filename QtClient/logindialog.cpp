#include "logindialog.h"
#include "ui_logindialog.h"
#include "global.h"
#include "httpmgr.h"
#include "tcpmgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->error_label->setProperty("state", "normal");
    ui->error_label->setText("");
    repolish(ui->error_label);

    connect(ui->reg_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    connect(ui->forget_btn, &QPushButton::clicked, this, &LoginDialog::switchForgetPassword);

    // 连接登录 HTTP 回包信号，回包后统一进入 slot_login_mod_finish 分发处理。
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_login_mod_finish, this,
            &LoginDialog::slot_login_mod_finish);

    // 登录成功后发出 TCP 连接信息，由 TcpMgr 建立长连接。
    connect(this, &LoginDialog::sig_connect_tcp,
            TcpMgr::GetInstance().get(), &TcpMgr::slot_tcp_connect);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success,
            this, &LoginDialog::slot_tcp_con_finish);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed,
            this, &LoginDialog::slot_tcp_login_failed);

    initHttpHandlers();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::initHttpHandlers()
{
    // 注册登录请求的回包处理逻辑。
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            if(error == ErrorCodes::UserNotExist){
                showTip(tr("用户名不存在"), false);
            }else if(error == ErrorCodes::PasswdErr || error == ErrorCodes::PasswdInvalid){
                showTip(tr("密码错误"), false);
            }else{
                showTip(tr("登录失败，错误码: %1").arg(error), false);
            }
            enableBtn(true);
            return;
        }

        auto user = jsonObj["user"].toString();
        showTip(tr("登录成功"), true);
        qDebug() << "user is " << user;

        // GateServer 登录成功后返回 ChatServer 的连接信息和身份凭证。
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();
        _uid = si.Uid;
        _token = si.Token;

        qDebug() << "user is " << user << " uid is " << si.Uid << " host is "
                 << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
    });
}

void LoginDialog::on_LoginButton_clicked()
{
    qDebug() << "login btn clicked";
    if(!checkPwdValid()) return;
    if(!checkUserValid()) return;
    enableBtn(false);

    // 发送登录 HTTP 请求。
    auto user = ui->user_edit->text().trimmed();
    auto pwd = ui->pass_edit->text().trimmed();
    QJsonObject json_login;
    json_login["user"] = user;
    json_login["passwd"] = pwd;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/user_login"),
                                        json_login, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}

bool LoginDialog::checkUserValid()
{
    auto user = ui->user_edit->text().trimmed();
    if(user.isEmpty()){
        qDebug() << "User empty";
        showTip(tr("用户名不能为空"), false);
        return false;
    }

    return true;
}

bool LoginDialog::checkPwdValid()
{
    auto pwd = ui->pass_edit->text();
    if(pwd.length() < 6){
        qDebug() << "Pass length invalid";
        showTip(tr("密码长度不能少于6位"), false);
        return false;
    }

    return true;
}

void LoginDialog::showTip(QString str, bool is_ok)
{
    ui->error_label->setProperty("state", is_ok ? "normal" : "err");
    ui->error_label->setText(str);
    repolish(ui->error_label);
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"), false);
        enableBtn(true);
        return;
    }

    // 解析 JSON 字符串，res 需要先转换为 QByteArray。
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"), false);
        enableBtn(true);
        return;
    }

    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"), false);
        enableBtn(true);
        return;
    }

    // 根据请求 ID 调用对应的处理函数，避免未知回包直接访问空 handler。
    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        it.value()(jsonDoc.object());
    } else {
        qDebug() << "no handler for req id:" << id;
    }
}
void LoginDialog::enableBtn(bool enabled)
{
    ui->LoginButton->setEnabled(enabled);
    ui->reg_Button->setEnabled(enabled);
    ui->forget_btn->setEnabled(enabled);
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if(bsuccess){
        showTip(tr("聊天服务连接成功，正在登录..."), true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

        // TCP 长连接建立成功后，再向 ChatServer 发送登录鉴权请求。
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);
    }else{
        showTip(tr("网络异常"), false);
        enableBtn(true);
    }
}

void LoginDialog::slot_tcp_login_failed(int err)
{
    switch (err) {
    case ErrorCodes::Error_Json:
        showTip(tr("聊天服务返回异常"), false);
        break;
    case ErrorCodes::Error_Param:
        showTip(tr("登录参数错误"), false);
        break;
    case ErrorCodes::UidInvalid:
        showTip(tr("用户状态已失效，请重新登录"), false);
        break;
    case ErrorCodes::TokenInvalid:
        showTip(tr("登录凭证已失效，请重新登录"), false);
        break;
    case ErrorCodes::RPCFailed:
        showTip(tr("聊天服务暂不可用"), false);
        break;
    default:
        showTip(tr("聊天登录失败，错误码: %1").arg(err), false);
        break;
    }

    enableBtn(true);
}



