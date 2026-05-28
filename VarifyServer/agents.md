# agents.md

## 项目概览

`VarifyServer` 是 Node.js gRPC 验证码服务。`GateServer` 通过 gRPC 调用 `GetVarifyCode(email)`，本服务生成 4 位验证码，将其写入 Redis 并设置过期时间，然后通过 SMTP 邮件发送给用户。

目录名沿用了项目现有拼写 `VarifyServer`。新增代码时请保持与现有引用一致，避免改名导致 C++ 侧配置或路径失效。

## 启动方式

安装依赖后启动：

```powershell
cd D:\Organized_Files\Projects\Active_Development\CPP_Boost\VarifyServer
npm install
node server.js
```

当前服务固定监听：

```text
0.0.0.0:50051
```

`GateServer` 的 `[VarifyServer] Host/Port` 需要与这里保持一致。

## 运行配置

`config.js` 从当前工作目录读取 `config.json`：

```json
{
  "email": {
    "user": "your@qq.com",
    "pass": "smtp-auth-code"
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "passwd": ""
  }
}
```

`email.pass` 通常是邮箱 SMTP 授权码，不应提交真实生产密钥。

## 代码结构

- `server.js`：gRPC 服务入口，注册 `GetVarifyCode`。
- `proto.js`：加载 `message.proto` 并导出 gRPC package。
- `message.proto`：验证码服务 RPC 定义。
- `redis.js`：Redis 连接和验证码写入、读取封装。
- `email.js`：nodemailer SMTP 发送封装。
- `config.js` / `config.json`：运行配置读取与本地配置。
- `const.js`：错误码和常量。

## 修改约定

1. 修改验证码 RPC 字段时，同步更新 `message.proto`，并更新 `GateServer` 侧生成的 protobuf/gRPC 文件。
2. 不要把真实邮箱授权码、Redis 密码提交到仓库。
3. 验证码 key 前缀由 `const.js` / `config.js` 中的 `code_` 约定决定，需与 `GateServer` 读取 `code_ + email` 保持一致。
4. 发送邮件失败时要返回明确错误码，避免 `GateServer` 误判验证码已经可用。

## 验证

至少运行：

```powershell
node server.js
```

然后通过 `GateServer` 的 `/get_varifycode` 接口或 gRPC 客户端调用 `GetVarifyCode`，确认 Redis 中出现 `code_<email>` 且 TTL 约为 300 秒。

