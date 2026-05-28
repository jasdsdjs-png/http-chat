#include "CSession.h"

#include "CServer.h"
#include "LogicSystem.h"

#include <boost/asio/detail/socket_ops.hpp>
#include <atomic>
#include <cstring>
#include <iostream>
#include <utility>

using namespace std;

MsgNode::MsgNode(short max_len) : _cur_len(0), _total_len(max_len) {
    // 多申请 1 字节是为了给字符串包体补 '\0'。
    // 网络协议本身不依赖 '\0'，但日志打印 JSON 字符串时会更方便。
    _data = new char[_total_len + 1];
    Clear();
}

MsgNode::~MsgNode() {
    delete[] _data;
}

void MsgNode::Clear() {
    // 每次复用缓冲区前清零，避免旧数据影响日志或字符串解析。
    ::memset(_data, 0, _total_len + 1);
    _cur_len = 0;
}

RecvNode::RecvNode(short max_len, short msg_id) : MsgNode(max_len), _msg_id(msg_id) {
}

SendNode::SendNode(const char* msg, short max_len, short msg_id)
    : MsgNode(max_len + HEAD_TOTAL_LEN), _msg_id(msg_id) {
    // 协议头必须使用网络字节序，保证不同 CPU 字节序的机器之间也能正确通信。
    short net_msg_id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
    short net_msg_len = boost::asio::detail::socket_ops::host_to_network_short(max_len);

    // SendNode 内存布局：
    // [0, 1]：消息 ID；
    // [2, 3]：包体长度；
    // [4, ...]：包体内容。
    ::memcpy(_data, &net_msg_id, HEAD_ID_LEN);
    ::memcpy(_data + HEAD_ID_LEN, &net_msg_len, HEAD_DATA_LEN);
    ::memcpy(_data + HEAD_TOTAL_LEN, msg, max_len);
}

LogicNode::LogicNode(shared_ptr<CSession> session, shared_ptr<RecvNode> recv_node)
    : _session(std::move(session)), _recv_node(std::move(recv_node)) {
}

CSession::CSession(boost::asio::io_context& io_context, CServer* server)
    : _socket(io_context),
      _server(server),
      _b_close(false),
      _authenticated(false),
      _uid(0),
      _recv_head_node(make_shared<MsgNode>(HEAD_TOTAL_LEN)) {
    // 使用自增编号作为连接 ID。
    // 这个 ID 不参与网络协议，只用于服务端内部 map 管理和日志排查。
    static atomic<unsigned long long> session_id{ 1 };
    _uuid = to_string(session_id.fetch_add(1));
}

CSession::~CSession() {
    cout << "session destruct, uuid is " << _uuid << endl;
}

tcp::socket& CSession::GetSocket() {
    return _socket;
}

string CSession::GetUuid() const {
    return _uuid;
}

void CSession::Start() {
    // 新连接的第一步永远是读消息头。
    // 只有拿到头部里的包体长度，才知道后面还要继续读多少字节。
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(const string& msg, short msg_id) {
    if (_b_close) {
        return;
    }

    if (msg.size() > MAX_LENGTH) {
        cout << "send message too long, size is " << msg.size() << endl;
        return;
    }

    auto send_node = make_shared<SendNode>(msg.c_str(), static_cast<short>(msg.size()), msg_id);
    bool pending = false;
    {
        lock_guard<mutex> lock(_send_lock);

        // 如果队列原本不为空，说明已有 async_write 正在发送。
        // 这时只入队，不再额外挂新的 async_write。
        pending = !_send_que.empty();
        _send_que.push(send_node);
    }

    // 同一条连接上同时只挂一个 async_write，避免多个写操作交叉导致包顺序错乱。
    // 当前写完成后，AsyncWrite 的回调会继续发送队列里的下一条消息。
    if (!pending) {
        AsyncWrite();
    }
}

void CSession::Close() {
    if (_b_close) {
        return;
    }

    _b_close = true;

    // shutdown/close 都可能因为连接已断开而返回错误。
    // 这里不抛异常，统一忽略错误码，保证清理流程能继续往下走。
    boost::system::error_code ec;
    _socket.shutdown(tcp::socket::shutdown_both, ec);
    _socket.close(ec);
}

void CSession::SetAuthenticated(int uid, const std::string& token) {
    lock_guard<mutex> lock(_auth_lock);
    _uid = uid;
    _token = token;
    _authenticated = true;
}

bool CSession::IsAuthenticated() const {
    lock_guard<mutex> lock(_auth_lock);
    return _authenticated;
}

int CSession::GetUid() const {
    lock_guard<mutex> lock(_auth_lock);
    return _uid;
}

void CSession::AsyncReadHead(int total_len) {
    auto self = shared_from_this();
    asyncReadFull(total_len, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                cout << "handle read failed, error is " << ec.what() << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            if (bytes_transfered < HEAD_TOTAL_LEN) {
                cout << "read length not match, read [" << bytes_transfered << "] , total ["
                    << HEAD_TOTAL_LEN << "]" << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            _recv_head_node->Clear();
            memcpy(_recv_head_node->_data, _data, bytes_transfered);

            // 头部前 2 个字节是消息 ID。
            // 客户端按网络字节序发送，服务端必须转成本机字节序后再使用。
            short msg_id = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            cout << "msg_id is " << msg_id << endl;

            if (msg_id <= 0) {
                cout << "invalid msg_id is " << msg_id << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            // 头部后 2 个字节是包体长度。
            // 读取包体前先做长度检查，防止恶意长度导致越界或过大内存申请。
            short msg_len = 0;
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
            cout << "msg_len is " << msg_len << endl;

            if (msg_len < 0 || msg_len > MAX_LENGTH) {
                cout << "invalid data length is " << msg_len << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            // 头部解析完成后，创建接收节点保存 msg_id 和后续包体。
            // 接下来进入第二阶段：按 msg_len 读取完整包体。
            _recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);
            AsyncReadBody(msg_len);
        }
        catch (std::exception& e) {
            cout << "Exception code is " << e.what() << endl;
        }
    });
}

void CSession::asyncReadFull(std::size_t max_length,
    function<void(const boost::system::error_code&, std::size_t)> handler) {
    ::memset(_data, 0, MAX_LENGTH);

    // 包体长度允许为 0。
    // 如果直接调用 async_read_some(buffer(..., 0))，不同平台表现不够直观；
    // 这里用 post 把“读完 0 字节”的结果投递回当前 io_context，保持异步回调语义一致。
    if (max_length == 0) {
        boost::asio::post(_socket.get_executor(), [handler]() {
            handler(boost::system::error_code(), 0);
        });
        return;
    }

    asyncReadLen(0, max_length, handler);
}

void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len,
    function<void(const boost::system::error_code&, std::size_t)> handler) {
    auto self = shared_from_this();
    _socket.async_read_some(boost::asio::buffer(_data + read_len, total_len - read_len),
        [read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t bytes_transfered) {
            if (ec) {
                handler(ec, read_len + bytes_transfered);
                return;
            }

            if (read_len + bytes_transfered >= total_len) {
                // 已经读满指定长度，交给上层解析完整头部或完整包体。
                handler(ec, read_len + bytes_transfered);
                return;
            }

            // TCP 是字节流，不保留应用层消息边界。
            // 一次 async_read_some 可能只读到半个包，所以长度不足时继续补读剩余字节。
            self->asyncReadLen(read_len + bytes_transfered, total_len, handler);
        });
}

void CSession::AsyncReadBody(int total_len) {
    auto self = shared_from_this();
    asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                cout << "handle read failed, error is " << ec.what() << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            if (bytes_transfered < static_cast<std::size_t>(total_len)) {
                cout << "read length not match, read [" << bytes_transfered << "] , total ["
                    << total_len << "]" << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            // 包体已经完整到达，复制到 RecvNode。
            // RecvNode 会被投递给逻辑线程，所以不能直接引用临时的 _data 缓冲。
            memcpy(_recv_msg_node->_data, _data, bytes_transfered);
            _recv_msg_node->_cur_len += static_cast<short>(bytes_transfered);
            _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
            cout << "receive data is " << _recv_msg_node->_data << endl;

            // 网络线程只负责收包；业务处理投递给逻辑线程，避免阻塞 IO 线程。
            // shared_from_this() 会延长当前 session 生命周期，保证逻辑层处理期间连接对象仍然有效。
            LogicSystem::GetInstance()->PostMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node));

            // 当前包处理完后，继续读取下一包的消息头。
            // 这就是长连接循环收包的关键：读头 -> 读体 -> 投递逻辑 -> 再读头。
            AsyncReadHead(HEAD_TOTAL_LEN);
        }
        catch (std::exception& e) {
            cout << "Exception code is " << e.what() << endl;
        }
    });
}

void CSession::AsyncWrite() {
    shared_ptr<SendNode> send_node;
    {
        lock_guard<mutex> lock(_send_lock);
        if (_send_que.empty()) {
            return;
        }
        send_node = _send_que.front();
    }

    auto self = shared_from_this();
    boost::asio::async_write(_socket,
        boost::asio::buffer(send_node->_data, send_node->_total_len),
        [self, this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                cout << "handle write failed, error is " << ec.what() << endl;
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            bool has_more = false;
            {
                lock_guard<mutex> lock(_send_lock);

                // 当前队首消息已经完整写出，可以从队列移除。
                _send_que.pop();
                has_more = !_send_que.empty();
            }

            // 如果业务线程在写出期间又放入了新消息，继续发送下一条。
            // 这里形成“一个写完成，再启动下一个写”的串行发送链。
            if (has_more) {
                AsyncWrite();
            }
        });
}
