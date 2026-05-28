#pragma once

#include "Singleton.h"
#include "const.h"

#include <condition_variable>
#include <mutex>
#include <thread>

// 每种消息 ID 对应一个业务处理函数。
// 参数分别是：来源连接、消息 ID、JSON 字符串包体。
using LogicCallBack = std::function<void(std::shared_ptr<CSession>, const short&, const std::string&)>;

// LogicSystem 是业务逻辑层。
// 网络线程收到完整包后只负责投递消息，不直接处理业务；
// LogicSystem 使用一个独立线程消费队列，避免耗时业务阻塞 IO 线程。
class LogicSystem : public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;

public:
    ~LogicSystem();

    // 网络层调用这个接口，把完整消息放入逻辑队列。
    void PostMsgToQue(std::shared_ptr<LogicNode> msg);

private:
    LogicSystem();

    // 注册 msg_id 到业务回调的映射。
    void RegisterCallBacks();

    // 逻辑线程主循环：等待队列消息，取出后根据 msg_id 分发。
    void DealMsg();

    // 登录消息处理函数。方案 A 中只做 JSON 解析和回显。
    void LoginHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data);

    std::queue<std::shared_ptr<LogicNode>> _msg_que;
    std::mutex _mutex;
    std::mutex _user_mutex;
    std::condition_variable _consume;
    std::thread _worker_thread;
    bool _b_stop;
    std::unordered_map<short, LogicCallBack> _fun_callbacks;
    std::unordered_map<int, std::shared_ptr<UserInfo>> _users;
};
