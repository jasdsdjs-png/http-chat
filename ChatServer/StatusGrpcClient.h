#pragma once

#include "ConfigMgr.h"
#include "Singleton.h"
#include "const.h"

#include "../GateServer/message.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class StatusConPool;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;

public:
    ~StatusGrpcClient();

    LoginRsp Login(int uid, const std::string& token);

private:
    StatusGrpcClient();

    std::unique_ptr<StatusConPool> pool_;
};

