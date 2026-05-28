#include "CServer.h"

#include "AsioIOServicePool.h"

#include <functional>
#include <iostream>

using namespace std;

CServer::CServer(boost::asio::io_context& io_context, short port)
    : _io_context(io_context),
      _port(port),
      _acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    cout << "Server start success, listen on port : " << _port << endl;

    // 构造完成后立即投递第一次 async_accept。
    // 后续每次 accept 完成，HandleAccept 末尾都会再次调用 StartAccept，
    // 这样服务器就能持续接收新的客户端连接。
    StartAccept();
}

CServer::~CServer() {
    cout << "server destruct" << endl;
}

void CServer::StartAccept() {
    // 监听动作由主 io_context 驱动，但新连接的 socket 绑定到线程池中的 io_context。
    // 这样做的好处是：accept 很轻，主线程继续专心接新连接；
    // 每条连接上的读写事件则分散到多个 IO 线程处理。
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
    shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);

    _acceptor.async_accept(new_session->GetSocket(),
        bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error) {
    if (!error) {
        // 新连接建立成功后，先让 session 开始读取消息头。
        new_session->Start();

        // CServer 持有 shared_ptr，保证连接活跃期间 CSession 不会提前析构。
        lock_guard<mutex> lock(_mutex);
        _sessions.insert(make_pair(new_session->GetUuid(), new_session));
    }
    else {
        cout << "session accept failed, error is " << error.what() << endl;
    }

    // 无论本次 accept 成功还是失败，都继续投递下一次 accept。
    // 这样单次连接错误不会让整个服务器停止接入新连接。
    StartAccept();
}

void CServer::ClearSession(string uuid) {
    // CSession 读写失败或主动关闭时会回调这里。
    // 从 map 中 erase 后，如果没有其他 shared_ptr 持有它，session 会自动析构。
    lock_guard<mutex> lock(_mutex);
    _sessions.erase(uuid);
}
