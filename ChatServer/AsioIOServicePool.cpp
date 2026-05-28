#include "AsioIOServicePool.h"

#include <iostream>

using namespace std;

AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : _ioServices(size == 0 ? 1 : size),
      _works(_ioServices.size()),
      _nextIOService(0) {
    // 给每个 io_context 创建 work guard，避免线程刚启动就因为没有任务而退出。
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _works[i] = std::make_unique<Work>(_ioServices[i].get_executor());
    }

    // 每个 io_context 绑定一个线程，用于处理会话上的异步读写事件。
    // 回调函数会在这些线程中执行，所以 CSession 内部共享数据需要加锁保护。
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
        });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    cout << "AsioIOServicePool destruct" << endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    // 简单轮询分配连接。
    // 例如有 4 个 IO 线程，第 1/5/9 个连接会分到同一个 io_context。
    auto& service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop() {
    // 先释放 work guard，让 io_context 在没有任务后可以自然退出。
    for (auto& work : _works) {
        if (work) {
            work.reset();
        }
    }

    // 再显式 stop，唤醒可能正在 run() 中等待事件的线程。
    for (auto& io_service : _ioServices) {
        io_service.stop();
    }

    // 最后 join 所有线程，保证退出 main 前没有后台线程继续访问已释放对象。
    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}
