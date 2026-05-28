#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// ============================================================================
// RPConPool — gRPC Stub 连接池
// ============================================================================
//
// 为什么需要连接池？
//   gRPC Channel（底层 TCP 连接）创建成本较高：
//     - TCP 三次握手 + TLS 握手（若加密）
//     - 每次新建连接 → 延迟增加、吞吐量下降
//   连接池预创建 N 个 Stub，请求线程获取→使用→归还，
//   避免每次 RPC 调用都重建连接。
//
// 线程安全设计：
//   - 入队/出队操作：std::mutex + std::condition_variable
//   - 关闭标志 b_stop_：std::atomic<bool>，防止唤醒后操作已销毁的队列
//   - getConnection() 阻塞等待：条件变量 wait，池中有可用连接或池关闭时唤醒
//
// 生命周期：
//   创建 → VerifyGrpcClient 构造函数中 new，预建 connections_ 队列
//   使用 → getConnection()/returnConnection() 取/还
//   销毁 → 析构函数 Close() 唤醒所有等待线程，清空队列
//
class RPConPool {
public:
    /**
     * @brief 构造函数：预创建 poolSize 个 gRPC Stub 并放入队列
     * @param poolSize 连接池大小（预建立的 Stub 数量）
     * @param host     远程 VerifyServer 主机地址（如 "127.0.0.1"）
     * @param port     远程 VerifyServer 端口号（如 "50051"）
     *
     * 初始化列表说明：
     *   - poolSize_(poolSize): 记录池容量，构造函数参数和成员用不同命名区分
     *   - host_(host):         保存远端主机地址（用于日志/调试）
     *   - port_(port):         保存远端端口
     *   - b_stop_(false):      原子布尔，false=池运行中，true=池已关闭
     *
     * 循环体内：
     *   1. grpc::CreateChannel(host:port) 创建 TCP 通道（非加密）
     *   2. VarifyService::NewStub(channel) 基于通道创建 Stub
     *   3. 将 Stub 放入 std::queue，等待后续取用
     */
    RPConPool(size_t poolSize, std::string host, std::string port)
        : poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
        for (size_t i = 0; i < poolSize_; ++i) {
            // 创建非加密的 gRPC 通道，连接地址格式：host:port
            std::shared_ptr<Channel> channel = grpc::CreateChannel(
                host + ":" + port,
                grpc::InsecureChannelCredentials()      // 非加密连接（内网环境）
            );
            // NewStub: 将 Channel 包装为指定服务的 Stub 对象
            // Stub 是自动生成的代理类，封装了底层序列化/网络传输
            connections_.push(VarifyService::NewStub(channel));
        }
    }

    /**
     * @brief 析构函数：安全关闭连接池并清空队列
     *
     * 执行顺序（不可颠倒！）：
     *   1. lock_guard 加锁 → 保证 Close() 与并发 getConnection() 互斥
     *   2. Close() → 设置 b_stop_=true，唤醒所有在 cond_.wait() 上阻塞的线程
     *   3. 循环 pop → 逐个释放 queue 中剩余的 unique_ptr<Stub>
     *      unique_ptr 出队后自动析构 → Stub 引用计数-1 → Channel 关闭
     */
    ~RPConPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
        while (!connections_.empty()) {
            connections_.pop();
        }
    }

    /**
     * @brief 从连接池获取一个可用的 Stub（阻塞等待）
     * @return std::unique_ptr<VarifyService::Stub>
     *         - 成功: 返回一个 Stub 的所有权
     *         - 池已关闭: 返回 nullptr
     *
     * 阻塞语义：
     *   cond_.wait(lock, predicate) 等价于：
     *     while (!predicate()) { cond_.wait(lock); }
     *
     *   唤醒条件（满足任一即唤醒）：
     *     1. b_stop_ == true       → 池正在关闭，返回 nullptr
     *     2. !connections_.empty() → 有可用连接，取出并返回
     *
     * 没有超时机制：若服务器一直不归还连接，调用者会永久阻塞
     * （生产环境建议加 std::condition_variable::wait_for 超时）
     */
    std::unique_ptr<VarifyService::Stub> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);  // unique_lock 支持条件变量
        cond_.wait(lock, [this] {
            if (b_stop_) {
                return true;            // 池关闭 → 唤醒（返回 nullptr）
            }
            return !connections_.empty(); // 有可用连接 → 唤醒
        });
        if (b_stop_) {
            return nullptr;             // 池已关闭，无可用连接
        }
        // 从队首取出一个 Stub（std::move 转移 unique_ptr 所有权）
        auto context = std::move(connections_.front());
        connections_.pop();
        return context;
    }

    /**
     * @brief 将用完的 Stub 归还到连接池
     * @param context 用完的 Stub（unique_ptr，所有权转移入池）
     *
     * 归还后的 Stub 可被其他调用者复用。
     * 若池已关闭（b_stop_==true），则直接丢弃该 Stub（出作用域自动析构）。
     */
    void returnConnection(std::unique_ptr<VarifyService::Stub> context) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;     // 池已关闭，丢弃归还的 Stub
        }
        connections_.push(std::move(context));
        cond_.notify_one();  // 通知一个等待的 getConnection() 线程
    }

    /**
     * @brief 关闭连接池，唤醒所有阻塞线程
     *
     * 设置 b_stop_=true + notify_all() 后：
     *   - 所有在 getConnection() 中等待的线程被唤醒
     *   - 各线程检查 b_stop_ 为 true，返回 nullptr
     *   - 新的 getConnection() 调用会立即返回 nullptr（b_stop_ 为真）
     */
    void Close() {
        b_stop_ = true;
        cond_.notify_all();      // 唤醒所有等待者，而非仅一个
    }

private:
    std::atomic<bool> b_stop_;                              // 关闭标志（原子变量，多线程安全读写）
    size_t poolSize_;                                       // 连接池容量（构造函数记录的副本）
    std::string host_;                                      // 远端主机地址
    std::string port_;                                      // 远端端口
    std::queue<std::unique_ptr<VarifyService::Stub>> connections_;  // Stub 队列（FIFO，公平分配）
    std::mutex mutex_;                                      // 保护队列操作的互斥锁
    std::condition_variable cond_;                          // 条件变量：空池等待 / 归还通知
};

// ============================================================================
// VerifyGrpcClient 成员函数实现
// ============================================================================

/**
 * @brief 发起 GetVarifyCode RPC 调用
 * @param email 目标邮箱
 * @return GetVarifyRsp（protobuf 消息，含 error 和 code 字段）
 *
 * 调用时序图：
 *
 *   VerifyGrpcClient::GetVarifyCode(email)
 *        │
 *        ├─[1] 构造 ClientContext（本次调用的元数据、超时等上下文）
 *        │
 *        ├─[2] pool_->getConnection()
 *        │       ├─ 池中有空闲 Stub → 取出并返回
 *        │       └─ 池空 → 线程阻塞，直到有归还或池关闭
 *        │            └─ 池关闭时返回 nullptr → reply.error = RPCFailed
 *        │
 *        ├─[3] stub->GetVarifyCode(&context, request, &reply)
 *        │       实际的 gRPC 同步调用（阻塞当前线程）
 *        │       ├─ Protobuf 序列化 request → 二进制
 *        │       ├─ HTTP/2 帧发送到远程 VarifyServer
 *        │       ├─ 等待远程响应
 *        │       ├─ HTTP/2 帧接收 + 反序列化 → reply
 *        │       └─ Status 对象记录调用成败
 *        │
 *        └─[4] pool_->returnConnection(move(stub))
 *               ├─ 无论 [3] 成功与否，Stub 必须归还
 *               └─ 还回的 Stub 可被下一个调用者复用
 */
GetVarifyRsp VerifyGrpcClient::GetVarifyCode(std::string email) {
    ClientContext context;      // 本地上下文（每次调用独立）
    GetVarifyRsp reply;         // 响应消息（栈上分配，拷贝返回）
    GetVarifyReq request;       // 请求消息
    request.set_email(email);   // 设置请求字段：要获取验证码的邮箱

    // 从连接池获取一个 Stub（阻塞式，直到有可用连接或池关闭）
    auto stub = pool_->getConnection();
    if (!stub) {
        // 连接池已关闭（通常发生在程序退出阶段）
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

    // 同步阻塞 RPC 调用
    // 参数: ClientContext* (可携带元数据/超时)
    //       request (protobuf 请求)
    //       &reply (protobuf 响应，输出参数)
    Status status = stub->GetVarifyCode(&context, request, &reply);

    // 无论 RPC 成功或失败，Stub 必须归还连接池
    // 否则池中连接逐渐耗尽 → 其他调用者永久阻塞
    if (status.ok()) {
        pool_->returnConnection(std::move(stub));
        return reply;
    }
    else {
        pool_->returnConnection(std::move(stub));
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
}

/**
 * @brief 析构函数 — 空实现，仅用于满足 PIMPL 惯用法
 *
 * 为什么需要显式析构函数？
 *   unique_ptr<RPConPool> 在析构时需要调用 delete RPConPool*，
 *   delete 不完整类型在 C++ 中属于未定义行为（UB）。
 *
 *   头文件中 RPConPool 仅有前向声明（class RPConPool;），
 *   若析构函数在头文件内联生成，编译器看到的是不完整类型 → 编译错误。
 *
 *   解决方案：
 *     1. 头文件中声明 ~VerifyGrpcClient()
 *     2. .cpp 中定义（此时 RPConPool 已完整定义）
 *     3. unique_ptr 的删除器在此处实例化，能正确调用 ~RPConPool()
 */
VerifyGrpcClient::~VerifyGrpcClient() {}

/**
 * @brief 构造函数：从配置读取远端地址并初始化连接池
 *
 * 构造流程（仅在 GetInstance() 首次调用时执行一次）：
 *   1. ConfigMgr::Inst()            获取全局配置管理器单例
 *   2. ["VarifyServer"]["Host"]     读取 config.ini 中 [VarifyServer] 段的 Host 键
 *   3. ["VarifyServer"]["Port"]     读取端口号
 *   4. new RPConPool(5, host, port) 创建含 5 个预连接 Stub 的连接池
 *
 * 配置示例（config.ini）：
 *   [VarifyServer]
 *   Host=127.0.0.1
 *   Port=50051
 */
VerifyGrpcClient::VerifyGrpcClient() {
    // gCfgMgr 是 ConfigMgr 的引用别名（auto& 推导）
    auto& gCfgMgr = ConfigMgr::Inst();

    // SectionInfo::operator[](key) 返回该 section 下 key 对应的 value
    std::string host = gCfgMgr["VarifyServer"]["Host"];
    std::string port = gCfgMgr["VarifyServer"]["Port"];

    // unique_ptr::reset(new T(...))：用新构造的 RPConPool 替换 pool_
    // 等价于 pool_ = std::make_unique<RPConPool>(5, host, port)
    pool_.reset(new RPConPool(5, host, port));
}
