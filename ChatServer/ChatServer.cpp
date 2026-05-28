#include "AsioIOServicePool.h"
#include "ConfigMgr.h"
#include "CServer.h"
#include "LogicSystem.h"

#include <boost/asio.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>

using namespace std;

int main() {
    try {
        // ConfigMgr 是单例，构造时会读取当前工作目录下的 config.ini。
        // 这里拿到 SelfServer.Port，决定 ChatServer 对外监听的端口。
        auto& cfg = ConfigMgr::Inst();

        // IO 线程池需要在 CServer 之前创建。
        // CServer accept 新连接后，会把每个 CSession 的 socket 分配到线程池中的某个 io_context。
        auto pool = AsioIOServicePool::GetInstance();

        // 主 io_context 只负责两件事：
        // 1. 监听新连接；
        // 2. 监听 SIGINT/SIGTERM，收到退出信号后停止服务。
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, pool](const boost::system::error_code&, int) {
            io_context.stop();
            pool->Stop();
        });

        auto port_str = cfg["SelfServer"]["Port"];
        CServer server(io_context, static_cast<short>(atoi(port_str.c_str())));

        // run() 会阻塞当前线程，直到 io_context 被 stop 或所有异步任务结束。
        io_context.run();
    }
    catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
