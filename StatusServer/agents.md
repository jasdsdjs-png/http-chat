# agents.md

## 项目概览

`StatusServer` 是聊天系统中的 gRPC 状态服务。`GateServer` 在用户 HTTP 登录成功后调用本服务，为用户分配一个可连接的 `ChatServer` 地址，并生成登录 token。后续如果接入真正的 ChatServer token 校验，也应以这里维护的分配和 token 语义为准。

## 构建方式

推荐使用 Visual Studio 2022 或 MSBuild：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'D:\Organized_Files\Projects\Active_Development\CPP_Boost\StatusServer\StatusServer.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /m
```

项目使用 C++17、gRPC、protobuf 和 Boost.PropertyTree。

## 运行配置

运行时从当前工作目录读取 `config.ini`：

```ini
[StatusServer]
Host=0.0.0.0
Port=50052

[ChatServers]
List=127.0.0.1:10087
```

`ChatServers.List` 支持逗号分隔的多个 `host:port`，例如：

```ini
List=127.0.0.1:10087,127.0.0.1:10088
```

## 代码结构

- `StatusServer.cpp`：gRPC 服务入口，读取配置，注册 `StatusService`。
- `StatusServerCore.h`：无网络依赖的核心业务逻辑，负责 ChatServer 轮询分配和 token 记录。
- `StatusServerCoreTests.cpp`：核心逻辑测试。
- `message.proto` / 生成的 `message.pb.*`、`message.grpc.pb.*`：与 `GateServer` 对齐的 gRPC 协议文件。

## 修改约定

1. 修改 gRPC 接口时，同时更新 `message.proto`，并重新生成 C++ protobuf/gRPC 文件。
2. 保持 `GateServer::StatusGrpcClient`、`ChatServer::StatusGrpcClient` 和本项目的接口字段一致，尤其是 `error`、`host`、`port`、`token`。
3. 纯业务逻辑优先放进 `StatusServerCore.h`，便于用 `StatusServerCoreTests.cpp` 单独测试。
4. 新增源文件后，记得加入 `StatusServer.vcxproj` 和 `StatusServer.vcxproj.filters`。

## ChatServer Contract

`GetChatServer(uid)` returns the `host`/`port` that `GateServer` gives to the
client, plus a token. After the client connects to ChatServer, ChatServer calls
`Login(uid, token)` back into this service.

A successful ChatServer login requires `uid > 0`, a non-empty token, an existing
`tokens_[uid]` record, and an exact token match. `Login` distinguishes missing
or invalid uid from token mismatch with separate error codes:

- `StatusUidInvalid = 1012`
- `StatusTokenInvalid = 1013`

Current token storage is
in-memory, so restarting StatusServer invalidates issued tokens. For
multi-instance or restart-safe login, move token storage to Redis with a TTL
while keeping `Login` as the single validation RPC.

## 验证

至少运行 Debug x64 构建。若修改了 `StatusServerCore`，同时构建或运行 `StatusServerCoreTests.cpp` 对应测试目标。
