# AGENTS.md

## StatusServer Login Verification

ChatServer now validates `MSG_CHAT_LOGIN` through `StatusServer` instead of
echoing the JSON body. The client should send:

```json
{"uid":1,"token":"token-from-gateserver"}
```

Protocol IDs are aligned with the Qt client:

- `MSG_CHAT_LOGIN = 1005`
- `MSG_CHAT_LOGIN_RSP = 1006`

`LogicSystem::LoginHandler` calls `StatusGrpcClient::Login(uid, token)`. After
the token passes, it loads `UserInfo` from the in-memory `_users` cache or from
MySQL through `MysqlMgr::GetUser(uid)`. The session is marked authenticated only
after both token verification and user loading succeed.

Runtime config must include:

```ini
[StatusServer]
Host=127.0.0.1
Port=50052

[Mysql]
Host=127.0.0.1
Port=3308
Passwd=123456
User=gatesvr
Schema=zzc
```

`ChatServer.vcxproj` compiles `StatusGrpcClient.cpp` and reuses
`../GateServer/message.pb.*` plus `../GateServer/message.grpc.pb.*`. Regenerate
the GateServer protobuf files before rebuilding ChatServer when the
`StatusService` proto changes.

Successful `MSG_CHAT_LOGIN_RSP` body:

```json
{"error":0,"uid":1,"token":"token-from-gateserver","name":"user-name","msg":"login success"}
```

The Qt client should register a handler for response ID `1006`, then store
`uid`, `name`, and `token` after `error == 0`.

## 项目概览

这是一个基于 Boost.Asio 的 C++ TCP 长连接 ChatServer 示例项目。

当前服务器实现的是最小可运行版本：

- 主线程负责监听客户端连接。
- `AsioIOServicePool` 提供多个 `io_context` 和 IO 线程。
- `CServer` 负责 `accept` 新连接并管理 `CSession` 生命周期。
- `CSession` 负责 TCP 拆包、收包、投递逻辑队列和异步回包。
- `LogicSystem` 使用独立线程消费业务消息队列。
- 当前已实现 `MSG_CHAT_LOGIN = 1001`，包体是 JSON，服务端解析成功后原样回显。

## 构建方式

推荐使用 Visual Studio 2022 或 MSBuild 构建：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'D:\Organized_Files\Projects\Active_Development\CPP_Boost\ChatServer\ChatServer.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /m
```

项目要求：

- C++17
- `/utf-8`
- Boost
- jsoncpp
- redis++ / hiredis 链接配置来自 `..\GateServer\PropertySheet.props`

`ChatServer.vcxproj` 已配置构建后复制 `jsoncpp.dll` 到输出目录。

## 运行配置

运行时读取当前工作目录下的 `config.ini`：

```ini
[SelfServer]
Port=10087
```

Visual Studio/MSBuild 构建时会把 `config.ini` 复制到输出目录。

注意：本机 `10086` 可能被其他程序占用，因此当前默认端口使用 `10087`。

## TCP 协议

每个应用层包由固定 4 字节头部和 JSON 包体组成：

```text
[0, 1]  消息 ID，short，网络字节序
[2, 3]  包体长度，short，网络字节序
[4, n]  JSON 包体
```

当前支持：

```cpp
MSG_CHAT_LOGIN = 1001
```

示例包体：

```json
{"uid":1,"token":"abc"}
```

服务端当前会返回解析后的 JSON。

## 代码结构

- `ChatServer.cpp`
  程序入口，读取配置、创建 IO 线程池、启动 `CServer`、监听退出信号。

- `AsioIOServicePool.h/.cpp`
  IO 线程池。每个 `io_context` 对应一个线程，用 `executor_work_guard` 防止线程提前退出。

- `CServer.h/.cpp`
  TCP 监听器。负责持续 `async_accept`，并用 `_sessions` 保存活跃连接。

- `CSession.h/.cpp`
  单个客户端连接。负责读 4 字节头、按长度读包体、投递 `LogicSystem`、串行异步发送响应。

- `LogicSystem.h/.cpp`
  业务逻辑层。网络线程不直接处理业务，而是把完整消息投递到这里的队列。

- `const.h`
  协议常量、消息节点结构、逻辑任务节点。

- `ConfigMgr.h/.cpp`
  简单 INI 配置读取器。

- `Singleton.h`
  线程安全单例模板。

## 修改约定

1. 修改网络收发逻辑时，优先保持 `CSession` 的读包流程：
   `AsyncReadHead -> AsyncReadBody -> PostMsgToQue -> AsyncReadHead`。

2. 不要在 IO 线程里做耗时业务，例如数据库访问、RPC 调用或复杂 JSON 处理。
   这些逻辑应该放到 `LogicSystem` 或更后面的业务层。

3. 同一条连接上不要同时发起多个 `async_write`。
   当前发送队列保证“一个写完成后再写下一个”，避免响应包交叉。

4. 增加新消息时：
   - 在 `const.h` 增加消息 ID。
   - 在 `LogicSystem::RegisterCallBacks()` 注册回调。
   - 新增对应业务处理函数。

5. 所有新增中文注释应保持 UTF-8 编码。
   工程已启用 `/utf-8`，避免中文注释在 MSVC 下乱码。

6. 修改 `.cpp/.h` 后，确认文件已加入 `ChatServer.vcxproj` 和 `ChatServer.vcxproj.filters`。

## 验证清单

改动后至少运行：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'D:\Organized_Files\Projects\Active_Development\CPP_Boost\ChatServer\ChatServer.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /m
```

期望结果：

```text
已成功生成。
0 个警告
0 个错误
```

如果运行程序直接退出并返回 `-1073741515`，通常是缺少运行时 DLL。
优先检查输出目录是否有 `jsoncpp.dll`。

## 常见坑

- TCP 是字节流，一次读取不一定得到完整包，所以不能假设一次 `async_read_some` 就读满。
- 头部字段使用网络字节序，读写时必须转换。
- `CSession` 的异步回调中需要使用 `shared_from_this()` 延长生命周期。
- `LogicSystem` 析构时要先设置 `_b_stop`，再 `notify_one()` 唤醒工作线程。
- `config.ini` 的读取路径是进程当前工作目录，不一定是源码目录。
