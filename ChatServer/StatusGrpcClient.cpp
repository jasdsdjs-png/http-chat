#include "StatusGrpcClient.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

class StatusConPool {
public:
    StatusConPool(size_t pool_size, std::string host, std::string port)
        : b_stop_(false), host_(std::move(host)), port_(std::move(port)) {
        for (size_t i = 0; i < pool_size; ++i) {
            auto channel = grpc::CreateChannel(
                host_ + ":" + port_,
                grpc::InsecureChannelCredentials());
            connections_.push(StatusService::NewStub(channel));
        }
    }

    ~StatusConPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
        while (!connections_.empty()) {
            connections_.pop();
        }
    }

    std::unique_ptr<StatusService::Stub> GetConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            return b_stop_ || !connections_.empty();
        });

        if (b_stop_) {
            return nullptr;
        }

        auto stub = std::move(connections_.front());
        connections_.pop();
        return stub;
    }

    void ReturnConnection(std::unique_ptr<StatusService::Stub> stub) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_ || !stub) {
            return;
        }

        connections_.push(std::move(stub));
        cond_.notify_one();
    }

    void Close() {
        b_stop_ = true;
        cond_.notify_all();
    }

private:
    std::atomic<bool> b_stop_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<StatusService::Stub>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

LoginRsp StatusGrpcClient::Login(int uid, const std::string& token) {
    ClientContext context;
    LoginReq request;
    LoginRsp reply;

    request.set_uid(uid);
    request.set_token(token);

    auto stub = pool_->GetConnection();
    if (!stub) {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

    Status status = stub->Login(&context, request, &reply);
    pool_->ReturnConnection(std::move(stub));

    if (!status.ok()) {
        reply.set_error(ErrorCodes::RPCFailed);
    }

    return reply;
}

StatusGrpcClient::~StatusGrpcClient() {}

StatusGrpcClient::StatusGrpcClient() {
    auto& cfg = ConfigMgr::Inst();
    std::string host = cfg["StatusServer"]["Host"];
    std::string port = cfg["StatusServer"]["Port"];
    if (host.empty()) {
        host = "127.0.0.1";
    }
    if (port.empty()) {
        port = "50052";
    }

    pool_.reset(new StatusConPool(5, host, port));
}

