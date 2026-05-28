#include "StatusGrpcClient.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <utility>

// 线程安全的 StatusService stub 连接池。
// stub 是 gRPC 自动生成的本地代理对象，用它来调用 StatusServer 的远程方法。
class StatusConPool {
public:
    StatusConPool(size_t poolSize, std::string host, std::string port)
        : b_stop_(false), poolSize_(poolSize), host_(std::move(host)), port_(std::move(port)) {
        for (size_t i = 0; i < poolSize_; ++i) {
            // 为连接池中的每个槽位创建一个 Channel 和一个 stub。
            // 这里使用非加密连接，适合教程或内网服务之间的通信。
            std::shared_ptr<Channel> channel = grpc::CreateChannel(
                host_ + ":" + port_,
                grpc::InsecureChannelCredentials());
            connections_.push(StatusService::NewStub(channel));
        }
    }

    ~StatusConPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        // 销毁队列前先唤醒所有可能正在等待连接的线程。
        Close();
        while (!connections_.empty()) {
            connections_.pop();
        }
    }

    std::unique_ptr<StatusService::Stub> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待直到池中有可用 stub，或者连接池正在关闭。
        cond_.wait(lock, [this] {
            if (b_stop_) {
                return true;
            }
            return !connections_.empty();
        });

        if (b_stop_) {
            return nullptr;
        }

        // 将队首 stub 的所有权移交给调用者。
        // 调用者完成 RPC 后必须通过 returnConnection 归还。
        auto stub = std::move(connections_.front());
        connections_.pop();
        return stub;
    }

    void returnConnection(std::unique_ptr<StatusService::Stub> stub) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 如果连接池已经关闭，就不再回收该 stub，让 unique_ptr 自动释放。
        if (b_stop_ || !stub) {
            return;
        }
        connections_.push(std::move(stub));
        cond_.notify_one();
    }

    void Close() {
        // 标记连接池关闭，并唤醒所有阻塞在 getConnection 的线程。
        b_stop_ = true;
        cond_.notify_all();
    }

private:
    std::atomic<bool> b_stop_;
    size_t poolSize_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<StatusService::Stub>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    // ClientContext 保存一次 RPC 调用的上下文信息，比如元数据、超时和取消状态。
    // 每次 RPC 调用都应该创建新的 ClientContext，不能复用。
    ClientContext context;
    GetChatServerRsp reply;
    GetChatServerReq request;
    request.set_uid(uid);

    // 从连接池借出一个 StatusService stub。
    auto stub = pool_->getConnection();
    if (!stub) {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

    // 同步 RPC 调用：GateServer 会在这里等待 StatusServer 返回结果或调用失败。
    // StatusServer 会把 host、port、token、error 等信息写入 reply。
    Status status = stub->GetChatServer(&context, request, &reply);

    // 无论 RPC 成功还是失败，都要归还 stub，方便后续登录请求复用。
    pool_->returnConnection(std::move(stub));

    if (status.ok()) {
        return reply;
    }

    reply.set_error(ErrorCodes::RPCFailed);
    return reply;
}

StatusGrpcClient::~StatusGrpcClient() {}

StatusGrpcClient::StatusGrpcClient()
{
    auto& cfg = ConfigMgr::Inst();
    // 从 config.ini 读取 StatusServer 地址：
    // [StatusServer]
    // Host = 127.0.0.1
    // Port = 50052
    std::string host = cfg["StatusServer"]["Host"];
    std::string port = cfg["StatusServer"]["Port"];

    // 创建一个小型连接池，应对多个登录请求并发访问 StatusServer。
    pool_.reset(new StatusConPool(5, host, port));
}
