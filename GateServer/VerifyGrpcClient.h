#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "const.h"
#include "SingleTon.h"

// ============================================================================
// gRPC 命名空间别名
// ============================================================================
// Channel:       gRPC 通道，代表与远程服务的一条 TCP 连接
// Status:        RPC 调用结果状态（成功/失败码 + 错误消息）
// ClientContext: RPC 调用上下文，传递元数据、超时、压缩等配置
using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

// ============================================================================
// Protobuf 消息别名（由 message.proto 生成）
// ============================================================================
// GetVarifyReq:   获取验证码请求，包含 email 字段
// GetVarifyRsp:   获取验证码响应，包含 error(错误码) 和 code(验证码) 字段
// VarifyService:  gRPC 服务 Stub，代理远程 VerifyServer 的 RPC 方法
using message::GetVarifyReq;
using message::GetVarifyRsp;
using message::VarifyService;

// 前向声明：gRPC Stub 连接池类（完整定义在 VerifyGrpcClient.cpp 中）
// 此处仅声明，头文件中用 unique_ptr 持有，避免暴露实现细节
class RPConPool;

/**
 * @brief gRPC 验证码客户端（单例 + 连接池）
 *
 * 职责：
 *   1. 封装对远程 VarifyServer 的 GetVarifyCode RPC 调用
 *   2. 内部维护一个 Stub 连接池（RPConPool），实现连接复用
 *   3. 避免每次 RPC 调用都新建 TCP 连接，提高并发性能
 *
 * 设计要点：
 *   - 继承 SingleTon<VerifyGrpcClient>：全局唯一实例，线程安全初始化
 *   - 持有 unique_ptr<RPConPool>：独占所有权，连接池生命周期与客户端绑定
 *   - 析构函数在 .cpp 中定义：因为 unique_ptr 析构需要 RPConPool 完整类型
 *     （头文件中仅有前向声明），这是 C++ PIMPL 惯用法
 *
 * 典型调用链：
 *   ConfigMgr::Inst()["VarifyServer"]["Host"]    // 配置管理器→读取远程地址
 *       ↓
 *   VerifyGrpcClient::GetInstance()               // 获取客户端单例
 *       ↓
 *   pool_->getConnection()                        // 从池中取 Stub
 *       ↓
 *   stub->GetVarifyCode(&ctx, request, &reply)   // 发起 gRPC 调用
 *       ↓
 *   pool_->returnConnection(move(stub))          // 用完归还 Stub
 */
class VerifyGrpcClient : public SingleTon<VerifyGrpcClient>
{
    // CRTP 友元：允许基类 SingleTon<T> 通过 new T 调用私有构造函数
    // 这是 CRTP 单例模式的标准写法
    friend class SingleTon<VerifyGrpcClient>;

public:
    /**
     * @brief 析构函数
     *
     * 必须在 .cpp 文件中定义（而非头文件内联）的原因：
     *   头文件中 RPConPool 仅有前向声明，属于不完整类型。
     *   std::unique_ptr<RPConPool> 的默认删除器需要 delete 指针，
     *   而 delete 不完整类型属于未定义行为（UB）。
     *   将析构函数定义推迟到 .cpp 中 → RPConPool 完整定义之后，
     *   unique_ptr 就能正确调用 RPConPool 的析构函数。
     */
    ~VerifyGrpcClient();

    /**
     * @brief 调用远程验证码服务获取验证码
     * @param email 目标邮箱地址，用于生成验证码
     * @return GetVarifyRsp 包含 error（0=成功）和 code（验证码字符串）
     *
     * 执行流程：
     *   1. 构造 gRPC ClientContext（生命周期仅限本次调用）
     *   2. 从连接池获取一个可用 Stub（若池空则阻塞等待）
     *   3. 通过 Stub 发起同步 RPC → 远程 VarifyServer
     *   4. 无论成功或失败，用完的 Stub 必须归还连接池
     */
    GetVarifyRsp GetVarifyCode(std::string email);

private:
    /**
     * @brief 私有构造函数（单例模式）
     *
     * 构造流程：
     *   1. 从 ConfigMgr 单例读取 VarifyServer 的 Host 和 Port 配置
     *   2. 创建 RPConPool 连接池（默认 5 个预连接）
     *
     * 仅由基类 SingleTon<VerifyGrpcClient>::GetInstance() 调用一次
     */
    VerifyGrpcClient();

    /**
     * @brief gRPC Stub 连接池（独占所有权）
     *
     * 使用场景：每次 GetVarifyCode 调用前从池中取 Stub，用完归还
     */
    std::unique_ptr<RPConPool> pool_;
};
