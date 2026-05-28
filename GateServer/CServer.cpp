#include "CServer.h"
#include "AsioIOServicePool.h"
#include <iostream>

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
    : _ioc(ioc),
      _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),//创建 + 设 IPv4 + 绑定端口 + 初始化acceptor
      _socket(ioc)// 相当于 socket() 创建了一个 fd
{
}

void CServer::Start() {
    auto self = shared_from_this();
    // 从线程池获取一个 io_context，新连接将绑定到这个 io_context 上运行
    // 这样每个连接的处理就分散到了线程池的不同线程中
    auto& pool_ioc = AsioIOServicePool::GetInstance()->GetIOService();

    // async_accept 的第一个参数传入线程池的 io_context（executor）
    // 这样 accept 到的新 socket 会自动在 pool_ioc 上创建
    // handler 的签名为 (error_code, tcp::socket)
    _acceptor.async_accept(pool_ioc, [self](beast::error_code ec, tcp::socket new_socket) {
        try {
            if (ec) {
                std::cout << "accept error: " << ec.message() << std::endl;
                self->Start();
                return;
            }
            // 新 socket 已经绑定到线程池的 io_context，后续所有读写都在该线程执行
            std::make_shared<HttpConnection>(std::move(new_socket))->Start();
            self->Start(); // 继续接受下一个连接
        }
        catch (std::exception& exp) {
            std::cout << "accept exception: " << exp.what() << std::endl;
            self->Start(); // 即使异常也要继续接受
        }
    });
}
