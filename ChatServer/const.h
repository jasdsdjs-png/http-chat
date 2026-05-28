#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <json/json.h>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

using boost::asio::ip::tcp;

// 单个数据包允许的最大包体长度。这里先限制为 2 KB，避免异常客户端一次性申请过大内存。
constexpr int MAX_LENGTH = 1024 * 2;

// 本项目的 TCP 应用层协议头固定 4 字节：
// [0, 1]：消息 ID，short 类型，网络字节序。
// [2, 3]：包体长度，short 类型，网络字节序。
// 头部后面紧跟指定长度的 JSON 字符串包体。
constexpr int HEAD_ID_LEN = 2;
constexpr int HEAD_DATA_LEN = 2;
constexpr int HEAD_TOTAL_LEN = HEAD_ID_LEN + HEAD_DATA_LEN;

// 当前服务器先实现一个最小登录消息。
// 客户端发送 msg_id=1001 和 JSON 包体，服务端解析成功后把 JSON 原样回给客户端。
constexpr short MSG_CHAT_LOGIN = 1005;
constexpr short MSG_CHAT_LOGIN_RSP = 1006;

enum ErrorCodes {
    Success = 0,
    RPCFailed = 1002,
    Error_Json = 1001,
    Error_Param = 1010,
    UserNotExist = 1011,
    UidInvalid = 1012,
    TokenInvalid = 1013,
};

struct UserInfo {
    int uid = 0;
    std::string name;
    std::string email;
    std::string pwd;
};

class CSession;

// MsgNode 是收发缓冲的基础结构。
// _total_len 表示这块缓冲区理论上应该容纳多少字节；
// _cur_len 表示当前已经写入或读取了多少字节；
// _data 额外申请 1 字节，用于在接收 JSON 字符串时手动补 '\0'，方便日志打印。
class MsgNode {
public:
    explicit MsgNode(short max_len);
    virtual ~MsgNode();
    void Clear();

    short _cur_len;
    short _total_len;
    char* _data;
};

// RecvNode 表示已经从客户端收到的一条完整消息。
// 相比 MsgNode，它多保存 msg_id，逻辑层可以根据 msg_id 找到对应的处理函数。
class RecvNode : public MsgNode {
public:
    RecvNode(short max_len, short msg_id);
    short _msg_id;
};

// SendNode 表示准备写回客户端的一条完整消息。
// 构造时会把消息 ID、包体长度和包体内容合并成一块连续内存，
// 后续 async_write 直接写这块内存即可。
class SendNode : public MsgNode {
public:
    SendNode(const char* msg, short max_len, short msg_id);
    short _msg_id;
};

// LogicNode 是网络层投递给逻辑层的任务。
// 它同时持有 session 和 recv_node：
// session 用来在业务处理结束后回包；
// recv_node 用来携带消息 ID 和包体内容。
class LogicNode {
public:
    LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recv_node);

    std::shared_ptr<CSession> _session;
    std::shared_ptr<RecvNode> _recv_node;
};
