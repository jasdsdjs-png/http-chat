#pragma once

#include "const.h"

#include <mutex>

class CServer;

// CSession 表示一个客户端连接。
// 它只关心这条 TCP 连接上的网络收发：
// 1. 按“4 字节头 + JSON 包体”的协议拆包；
// 2. 把完整消息投递给 LogicSystem；
// 3. 将 LogicSystem 生成的响应按同样协议写回客户端。
class CSession : public std::enable_shared_from_this<CSession> {
public:
    CSession(boost::asio::io_context& io_context, CServer* server);
    ~CSession();

    // acceptor 需要拿到 socket，才能把新连接绑定到当前 session。
    tcp::socket& GetSocket();

    // session 的唯一编号，用于 CServer 在 map 中保存和清理连接。
    std::string GetUuid() const;

    // 连接建立后调用，开始读取第一段消息头。
    void Start();

    // 线程安全地把响应放入发送队列。真正写 socket 的动作在 AsyncWrite 中串行执行。
    void Send(const std::string& msg, short msg_id);

    // 主动关闭连接。错误处理和服务退出时都会走这里。
    void Close();

    void SetAuthenticated(int uid, const std::string& token);
    bool IsAuthenticated() const;
    int GetUid() const;

private:
    // 读取固定 4 字节消息头，解析出 msg_id 和 body_len。
    void AsyncReadHead(int total_len);

    // 根据消息头中的 body_len 继续读取包体。
    void AsyncReadBody(int total_len);

    // 读取“恰好 max_length 字节”。async_read_some 可能一次读不满，所以这里会递归补读。
    void asyncReadFull(std::size_t max_length,
        std::function<void(const boost::system::error_code&, std::size_t)> handler);
    void asyncReadLen(std::size_t read_len, std::size_t total_len,
        std::function<void(const boost::system::error_code&, std::size_t)> handler);

    // 从发送队列中取出一个 SendNode，使用 boost::asio::async_write 写回客户端。
    void AsyncWrite();

    tcp::socket _socket;
    CServer* _server;
    std::string _uuid;
    char _data[MAX_LENGTH];
    bool _b_close;
    bool _authenticated;
    int _uid;
    std::string _token;
    mutable std::mutex _auth_lock;

    std::shared_ptr<MsgNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;

    // 发送队列用于保证同一连接上的响应按顺序写出。
    std::queue<std::shared_ptr<SendNode>> _send_que;
    std::mutex _send_lock;
};
