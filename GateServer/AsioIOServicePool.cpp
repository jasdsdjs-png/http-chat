#include "AsioIOServicePool.h"
#include <iostream>

AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : _ioServices(size)
    , _works(size)
    , _nextIOService(0)
{
    // 为每个 io_context 创建 work_guard，避免没有任务时 run() 立即退出。
    for (std::size_t i = 0; i < size; ++i) {
        _works[i] = std::make_unique<Work>(boost::asio::make_work_guard(_ioServices[i]));
    }

    // 每个 io_context 对应一个工作线程，负责执行异步任务回调。
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
        });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    std::lock_guard<std::mutex> lock(_mutex);
    // 通过轮询方式选择下一个 io_context，尽量让连接均匀分布到各线程。
    auto& service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop() {
    // 释放 work_guard，让 io_context 在任务处理完后可以退出。
    for (auto& work : _works) {
        work.reset();
    }

    // 主动停止所有 io_context，唤醒可能阻塞在 run() 中的线程。
    for (auto& ioc : _ioServices) {
        ioc.stop();
    }

    // 等待所有工作线程结束，避免析构时线程仍在运行。
    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}
