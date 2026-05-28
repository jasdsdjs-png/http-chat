# VarifyServer 项目逻辑梳理

## 项目定位

`VarifyServer` 是验证码发放服务。它被 `GateServer` 调用，不直接面向客户端。服务收到邮箱后生成短验证码，把验证码写入 Redis 并设置 5 分钟过期时间，再通过 SMTP 发送邮件，最后把处理结果通过 gRPC 返回给网关。

## 核心流程图

```mermaid
flowchart TD
    A[启动 server.js] --> B[加载 message.proto]
    B --> C[创建 gRPC Server]
    C --> D[注册 VarifyService.GetVarifyCode]
    D --> E[监听 0.0.0.0:50051]
    E --> F[GateServer 调用 GetVarifyCode(email)]
    F --> G[生成 uuid 并截取前 4 位]
    G --> H[写入 Redis: code_email -> code, TTL 300]
    H --> I{Redis 写入成功}
    I -- 否 --> J[返回 RedisErr 和空 code]
    I -- 是 --> K[组装邮件内容]
    K --> L[调用 nodemailer 发送邮件]
    L --> M{发送成功}
    M -- 是 --> N[返回 Success/email/code]
    M -- 否 --> O[捕获异常并返回 Exception]
```

## 文字说明

`server.js` 是服务入口。启动时通过 `proto.js` 加载 `message.proto`，拿到 `message.VarifyService`，然后把本地的 `GetVarifyCode` 函数注册为 gRPC handler。服务使用非加密 gRPC 监听 `0.0.0.0:50051`。

`GetVarifyCode(call, callback)` 从 `call.request.email` 取邮箱地址。当前实现用 `uuidv4()` 生成随机字符串，并截取前 4 位作为验证码。验证码写入 Redis 的 key 是 `code_` 加邮箱，过期时间是 300 秒。`GateServer` 注册、重置密码时也按同样 key 读取验证码，所以这个前缀是跨服务协议的一部分。

Redis 访问封装在 `redis.js`。`SetRedisExpire` 先 `set` 再 `expire`，成功返回 `true`。如果 Redis 写入失败，验证码服务不会继续发邮件，而是直接通过 gRPC 返回 `RedisErr`。

邮件发送封装在 `email.js`，底层使用 `nodemailer` 连接 QQ 邮箱 SMTP。`config.js` 从当前工作目录的 `config.json` 读取 SMTP 账号、授权码和 Redis 连接信息。发送邮件成功后，服务返回 `Success`、邮箱和验证码；发生异常时返回 `Exception` 和空验证码。

需要注意的是，当前 gRPC 返回里包含 `code` 字段，便于开发调试，但生产环境通常不应把验证码明文返回给前端，只应通过邮件送达。

## 关键文件

- `server.js`：gRPC 服务注册和验证码主流程。
- `proto.js`：protobuf 动态加载。
- `message.proto`：`VarifyService`、`GetVarifyReq`、`GetVarifyRsp` 定义。
- `redis.js`：Redis 客户端和验证码缓存操作。
- `email.js`：SMTP 邮件发送。
- `config.js`：读取 `config.json` 并导出配置。

