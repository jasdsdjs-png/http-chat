# GateServer 项目逻辑梳理

## 项目定位

`GateServer` 是聊天系统的 HTTP 网关。它直接面向客户端，负责验证码申请、用户注册、密码重置和登录入口。网关会调用 `VarifyServer` 获取验证码，访问 Redis 校验验证码，访问 MySQL 读写账号数据，并在登录成功后调用 `StatusServer` 获取可连接的 `ChatServer` 地址和 token。

## 核心流程图

```mermaid
flowchart TD
    A[进程启动 GateServer.cpp] --> B[读取 GateServer.Port]
    B --> C[启动 Boost.Beast HTTP Server]
    C --> D[预初始化 RedisMgr 和 MysqlMgr]
    D --> E[async_accept HTTP 连接]
    E --> F[HttpConnection async_read 请求]
    F --> G{HTTP 方法}
    G -- GET --> H[LogicSystem HandleGet]
    G -- POST --> I[LogicSystem HandlePost]
    G -- 其他 --> J[返回 method_not_allowed]
    H --> K[/get_test 返回查询参数]
    I --> L{POST 路由}
    L -- /get_varifycode --> M[调用 VerifyGrpcClient]
    M --> N[VarifyServer 生成验证码并写 Redis]
    N --> O[返回 email/code/ttl]
    L -- /user_register --> P[校验 JSON 和确认密码]
    P --> Q[从 Redis 读取 code_email]
    Q --> R{验证码正确}
    R -- 否 --> S[返回验证码错误或过期]
    R -- 是 --> T[调用 MysqlMgr RegUser]
    T --> U[返回 uid 或用户已存在]
    L -- /reset_pwd --> V[校验验证码后调用 MysqlMgr ResetPwd]
    L -- /user_login --> W[调用 MysqlMgr CheckPwd]
    W --> X{密码正确}
    X -- 否 --> Y[返回密码错误]
    X -- 是 --> Z[调用 StatusGrpcClient GetChatServer]
    Z --> AA[返回 uid/token/host/port]
```

## 文字说明

程序入口在 `GateServer.cpp`。启动时从 `config.ini` 读取 `[GateServer] Port`，创建单个主 `io_context` 负责监听 HTTP 端口，并预初始化 Redis 和 MySQL 连接池。`CServer` 接受新 TCP 连接后，把 socket 交给 `HttpConnection`。

`HttpConnection` 基于 Boost.Beast 读取 HTTP 请求，解析 URL 和 query string，并按 GET/POST 分发给 `LogicSystem`。连接设置了 60 秒 deadline，响应写出后关闭发送方向并取消超时器。当前响应默认不保持长连接。

`LogicSystem` 在构造函数里注册所有 HTTP 路由。GET 只提供 `/get_test` 用于回显查询参数；主要业务都在 POST 中：

- `/get_varifycode`：解析 `email`，调用 `VerifyGrpcClient::GetVarifyCode`，由验证码服务生成验证码、写入 Redis 并发送邮件。网关再补充 Redis TTL 返回给客户端。
- `/user_register`：校验 JSON、密码确认字段、Redis 中的验证码，然后调用 MySQL 存储过程 `reg_user` 创建用户。
- `/reset_pwd`：校验用户、邮箱、新密码确认和验证码，然后调用 MySQL 存储过程 `reset_pwd` 修改密码。
- `/user_login`：检查用户名和密码，成功后调用 `StatusGrpcClient::GetChatServer(uid)` 获取聊天服务器地址与登录 token。

`VerifyGrpcClient` 和 `StatusGrpcClient` 都使用 gRPC stub 连接池。每次请求从池中取一个 stub，完成同步 RPC 后归还，减少重复创建连接的成本。Redis 使用 redis++ 连接池，MySQL 使用自定义 `MySqlPool` 和 Connector/C++。

## 关键文件

- `GateServer.cpp`：进程入口、端口读取、HTTP 服务启动、Redis/MySQL 初始化。
- `CServer.*`：HTTP TCP acceptor。
- `HttpConnection.*`：HTTP 读请求、URL 解析、路由分发、写响应。
- `LogicSystem.*`：HTTP 业务路由和账号流程。
- `VerifyGrpcClient.*`：调用验证码 gRPC 服务。
- `StatusGrpcClient.*`：调用状态 gRPC 服务获取 ChatServer。
- `MysqlMgr.*`、`MysqlDao.*`：用户注册、密码重置、登录校验。
- `RedisMgr.*`：验证码缓存读取和 TTL 查询。

