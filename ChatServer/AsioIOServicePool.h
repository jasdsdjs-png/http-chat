#pragma once

#include "Singleton.h"

#include <boost/asio.hpp>
#include <thread>
#include <vector>

// AsioIOServicePool 管理一组 io_context 和工作线程。
// 主线程只负责 accept 新连接；每个 CSession 会被分配到池中的某个 io_context，
// 连接上的 async_read/async_write 回调就由对应线程执行。
class AsioIOServicePool : public Singleton<AsioIOServicePool> {
    friend class Singleton<AsioIOServicePool>;

public:
    using IOService = boost::asio::io_context;

    // executor_work_guard 的作用是“占住”io_context。
    // 如果没有 work guard，io_context 没有任务时 run() 会立刻返回，线程也会退出；
    // 有了它，线程会一直等待后续投递进来的异步事件。
    using Work = boost::asio::executor_work_guard<IOService::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

    // 使用 round-robin 的方式返回一个 io_context，让不同连接分散到多个 IO 线程。
    boost::asio::io_context& GetIOService();

    // 停止所有 io_context，并等待线程退出。
    void Stop();

private:
    explicit AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());

    std::vector<IOService> _ioServices;
    std::vector<WorkPtr> _works;
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};
