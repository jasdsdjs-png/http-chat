# StatusServer 项目逻辑梳理

## 项目定位

`StatusServer` 是内部 gRPC 服务，主要职责是给登录成功的用户分配一个 `ChatServer`，并生成 token。它不直接处理 HTTP，也不直接连接 MySQL/Redis；它面向 `GateServer` 提供轻量的状态分配接口。

## 核心流程图

```mermaid
flowchart TD
    A[进程启动 StatusServer.cpp] --> B[读取 config.ini]
    B --> C[解析 StatusServer Host/Port]
    B --> D[解析 ChatServers.List]
    D --> E{列表为空}
    E -- 是 --> F[使用默认 127.0.0.1:10087]
    E -- 否 --> G[保存配置中的 ChatServer 列表]
    F --> H[创建 StatusServiceImpl]
    G --> H
    H --> I[注册 gRPC StatusService]
    I --> J[监听 Host:Port]
    J --> K{收到 RPC}
    K -- GetChatServer(uid) --> L{uid 合法且有 ChatServer}
    L -- 否 --> M[返回参数错误]
    L -- 是 --> N[按轮询选择 ChatServer]
    N --> O[生成 token 并保存 tokens_[uid]]
    O --> P[返回 host/port/token]
    K -- Login(uid, token) --> Q[检查 tokens_ 中 uid/token 是否匹配]
    Q --> R[返回校验结果]
```

## 文字说明

`StatusServer.cpp` 负责进程启动和 gRPC 绑定。服务先通过 Boost.PropertyTree 读取当前工作目录下的 `config.ini`，其中 `[StatusServer]` 决定 gRPC 监听地址，`[ChatServers] List` 决定可分配的聊天服务器列表。

`ParseChatServers` 将逗号分隔的 `host:port` 字符串解析成 `ChatServerEndpoint` 列表。若配置缺失或解析结果为空，服务会回退到 `127.0.0.1:10087`，保证开发环境下仍能启动。

真正的业务逻辑集中在 `StatusServiceCore`。`GetChatServer(uid)` 首先校验 uid 和服务器列表，然后用 `next_index_` 原子递增实现简单轮询，把用户分配到某个 ChatServer。随后 `GenerateToken(uid)` 生成 token，并将 `uid -> token` 写入内存 map。

`Login(uid, token)` 用于校验 token 是否与内存中记录一致。`GateServer` 在 HTTP 登录成功后先调用 `GetChatServer` 获取 token；客户端随后连接 `ChatServer`，`ChatServer` 再调用本服务的 `Login(uid, token)` 完成二次校验。只有 uid 合法、token 非空、且与 `tokens_[uid]` 完全一致时才返回成功。uid 不存在或参数无效返回 `StatusUidInvalid = 1012`，token 不匹配返回 `StatusTokenInvalid = 1013`。

`tokens_` 由 mutex 保护，因此并发 gRPC 请求下 token 写入和读取不会同时修改 map。需要注意的是，当前 token 存在内存中，服务重启后会丢失；如果要做多实例或持久登录态，应迁移到 Redis 等共享存储，并设置 token TTL。迁移时应保持 `Login` RPC 作为 ChatServer 的统一校验入口。

## 关键文件

- `StatusServer.cpp`：配置读取、gRPC 服务实现、服务启动。
- `StatusServerCore.h`：ChatServer 分配、token 生成和 token 校验。
- `StatusServerCoreTests.cpp`：核心逻辑测试入口。
