#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>
#include "SingleTon.h"
#include "const.h"

// Asio IO 上下文线程池（单例）
// 创建 N 个 io_context 实例，每个实例运行在独立线程中。
// 使用轮询方式将新的连接分配到不同线程。
class AsioIOServicePool : public SingleTon<AsioIOServicePool>
{
    friend SingleTon<AsioIOServicePool>;

public:
    using IOService = boost::asio::io_context;
    //让对应的 io_context.run() 即使当前没有异步任务，也不要立刻退出。
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;
    ~AsioIOServicePool();

    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

    // 轮询返回下一个 io_context 引用。
    boost::asio::io_context& GetIOService();

    // 停止所有 io_context 实例，并等待工作线程结束。
    void Stop();

private:
    AsioIOServicePool(std::size_t size = 2/*std::thread::hardware_concurrency()*/);

    std::vector<IOService>   _ioServices;
    std::vector<WorkPtr>     _works;
    std::vector<std::thread> _threads;
    std::size_t              _nextIOService;
    std::mutex               _mutex;
};
