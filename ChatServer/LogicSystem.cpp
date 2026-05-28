#include "LogicSystem.h"

#include "CSession.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"

#include <iostream>
#include <utility>

using namespace std;

LogicSystem::LogicSystem() : _b_stop(false) {
    // 先注册所有业务处理函数，再启动消费线程。
    // 这样可以保证线程开始取消息时，回调表已经可用。
    RegisterCallBacks();
    _worker_thread = thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem() {
    {
        lock_guard<mutex> lock(_mutex);
        _b_stop = true;
    }

    // 唤醒可能正在 wait 的逻辑线程，让它检查 _b_stop 并退出。
    _consume.notify_one();

    if (_worker_thread.joinable()) {
        _worker_thread.join();
    }
}

void LogicSystem::RegisterCallBacks() {
    // 当前只注册登录消息。
    // 后续如果新增聊天、心跳、好友列表等消息，只需要继续往 _fun_callbacks 里加映射。
    _fun_callbacks[MSG_CHAT_LOGIN] = bind(&LogicSystem::LoginHandler, this,
        placeholders::_1, placeholders::_2, placeholders::_3);
}

void LogicSystem::PostMsgToQue(shared_ptr<LogicNode> msg) {
    {
        lock_guard<mutex> lock(_mutex);
        _msg_que.push(std::move(msg));
    }

    // 队列写入完成后唤醒逻辑线程。
    // notify 放在锁外也可以，这里放在锁后，避免线程醒来后马上抢同一把锁。
    _consume.notify_one();
}

void LogicSystem::DealMsg() {
    for (;;) {
        shared_ptr<LogicNode> msg_node;
        {
            unique_lock<mutex> lock(_mutex);

            // 条件变量用于避免空队列时 CPU 空转。
            // 只有收到新消息，或析构时设置 _b_stop，线程才会继续往下执行。
            _consume.wait(lock, [this]() {
                return _b_stop || !_msg_que.empty();
            });

            if (_b_stop && _msg_que.empty()) {
                break;
            }

            msg_node = _msg_que.front();
            _msg_que.pop();
        }

        auto recv_node = msg_node->_recv_node;
        string msg_data(recv_node->_data, recv_node->_cur_len);

        // 根据消息 ID 找业务回调。
        // 找不到说明客户端发了当前服务器不支持的消息，记录日志后丢弃。
        auto iter = _fun_callbacks.find(recv_node->_msg_id);
        if (iter == _fun_callbacks.end()) {
            cout << "msg id [" << recv_node->_msg_id << "] handler not found" << endl;
            continue;
        }

        // 真正的业务处理在逻辑线程里执行。
        // 这样网络线程只负责收包和发包，不会被 JSON 解析、数据库访问等业务耗时阻塞。
        iter->second(msg_node->_session, recv_node->_msg_id, msg_data);
    }
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
    Json::Reader reader;
    Json::Value root;

    // 当前协议约定包体是 JSON 字符串。
    // JSON 解析失败时仍然按同一个 msg_id 回包，方便客户端把响应关联回原请求。
    if (!reader.parse(msg_data, root)) {
        Json::Value rsp;
        rsp["error"] = ErrorCodes::Error_Json;
        rsp["msg"] = "invalid json";
        session->Send(rsp.toStyledString(), MSG_CHAT_LOGIN_RSP);
        return;
    }

    int uid = root["uid"].asInt();
    string token = root["token"].asString();
    if (uid <= 0 || token.empty()) {
        Json::Value rsp;
        rsp["error"] = ErrorCodes::Error_Param;
        rsp["msg"] = "uid or token is invalid";
        session->Send(rsp.toStyledString(), MSG_CHAT_LOGIN_RSP);
        return;
    }

    cout << "user login uid is " << uid << " user token is " << token << endl;

    auto login_rsp = StatusGrpcClient::GetInstance()->Login(uid, token);
    if (login_rsp.error() != ErrorCodes::Success) {
        Json::Value rsp;
        rsp["error"] = login_rsp.error();
        rsp["msg"] = "token verify failed";
        session->Send(rsp.toStyledString(), MSG_CHAT_LOGIN_RSP);
        return;
    }

    shared_ptr<UserInfo> user_info;
    {
        lock_guard<mutex> lock(_user_mutex);
        auto iter = _users.find(uid);
        if (iter != _users.end()) {
            user_info = iter->second;
        }
    }

    if (!user_info) {
        user_info = MysqlMgr::GetInstance()->GetUser(uid);
        if (!user_info) {
            Json::Value rsp;
            rsp["error"] = ErrorCodes::UidInvalid;
            rsp["msg"] = "user not found";
            session->Send(rsp.toStyledString(), MSG_CHAT_LOGIN_RSP);
            return;
        }

        lock_guard<mutex> lock(_user_mutex);
        _users[uid] = user_info;
    }

    session->SetAuthenticated(uid, token);

    Json::Value rsp;
    rsp["error"] = ErrorCodes::Success;
    rsp["uid"] = login_rsp.uid();
    rsp["token"] = login_rsp.token();
    rsp["name"] = user_info->name;
    rsp["msg"] = "login success";
    session->Send(rsp.toStyledString(), MSG_CHAT_LOGIN_RSP);
}
