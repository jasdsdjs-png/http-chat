#ifndef GLOBAL_H
#define GLOBAL_H

#include <QByteArray>
#include <QDir>
#include <QJsonObject>
#include <QMap>
#include <QNetworkReply>
#include <QStyle>
#include <QWidget>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

extern QString gate_url_prefix;

// 全局 QSS 刷新函数。
// 这里只做 extern 声明，实际定义在 global.cpp 中。
extern std::function<void(QWidget*)> repolish;

// 请求 ID：用于区分 HTTP 请求和 TCP 消息的业务类型。
enum ReqId{
    ID_GET_VARIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002,        // 注册用户
    ID_RESET_PWD = 1003,       // 重置密码
    ID_LOGIN_USER = 1004,      // HTTP 登录 GateServer
    ID_CHAT_LOGIN = 1005,      // TCP 登录 ChatServer
    ID_CHAT_LOGIN_RSP = 1006,  // ChatServer 登录响应
};

// 业务模块 ID：HttpMgr 根据模块把 HTTP 回包分发给对应页面。
enum Modules{
    REGISTERMOD = 0, // 注册模块
    RESETPWDMOD = 1, // 重置密码模块
    LOGINMOD = 2,    // 登录模块
};

// 统一错误码：包含客户端本地错误和服务端业务错误。
enum ErrorCodes{
    SUCCESS = 0,          // 成功
    ERR_JSON = 1,         // 本地 JSON 解析失败
    ERR_NETWORK = 2,      // 本地网络错误
    Error_Json = 1001,    // 服务端 JSON 解析失败
    RPCFailed = 1002,     // RPC 调用失败
    VarifyExpired = 1003, // 验证码已过期
    VarifyCodeErr = 1004, // 验证码错误
    UserExist = 1005,     // 用户已存在
    PasswdErr = 1006,     // 密码错误或不匹配
    EmailNotMatch = 1007, // 邮箱不匹配
    PasswdUpFailed = 1008,// 密码更新失败
    PasswdInvalid = 1009, // 密码无效
    Error_Param = 1010,   // 参数错误
    UserNotExist = 1011,  // 用户不存在
    UidInvalid = 1012,    // uid 无效或 token 不存在
    TokenInvalid = 1013,  // token 不匹配
};

// 可点击标签的显示状态。
enum ClickLbState{
    Normal = 0,   // 普通状态
    Selected = 1, // 选中状态
};

// ChatServer 连接信息。
// GateServer 登录成功后返回这些字段，LoginDialog 保存 uid/token，TcpMgr 使用 Host/Port 建立 TCP 连接。
struct ServerInfo
{
    int Uid;        // 当前登录用户 ID
    QString Host;   // ChatServer 主机名或 IP
    QString Port;   // ChatServer 端口，连接前转换为整数
    QString Token;  // 登录凭证，TCP 连接成功后发送给 ChatServer 做鉴权
};

#endif // GLOBAL_H
