#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>

#include "ConfigMgr.h"
#include "SingleTon.h"
#include "const.h"
#include "message.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

// StatusConPool 的具体实现放在 .cpp 文件中。
// 头文件里只做前向声明，避免把连接池细节暴露给 StatusGrpcClient 的使用者。
class StatusConPool;

// GateServer 在用户密码校验通过后调用 StatusGrpcClient。
// 它会向 StatusServer 询问：该用户应该连接哪台 ChatServer，
// 并获取登录 token，最后这些信息会返回给 Qt 客户端。
class StatusGrpcClient : public SingleTon<StatusGrpcClient>
{
    // SingleTon<T> 需要访问私有构造函数来创建单例对象。
    friend class SingleTon<StatusGrpcClient>;

public:
    ~StatusGrpcClient();

    // 根据用户 uid 请求一台可用的聊天服务器。
    // 成功时返回 host、port、token。
    // RPC 失败时会把响应里的 error 设置为 ErrorCodes::RPCFailed。
    GetChatServerRsp GetChatServer(int uid);

private:
    StatusGrpcClient();

    // 可复用的 StatusService stub 连接池。
    // 每个 stub 内部封装了一条到 StatusServer 的 gRPC 通道，
    // 这样登录请求不用每次都重新创建连接。
    std::unique_ptr<StatusConPool> pool_;
};
