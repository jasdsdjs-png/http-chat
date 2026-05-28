# AGENTS.md

## Project Overview

GateServer is the HTTP entry service for the chat system. It accepts client HTTP
requests, talks to VerifyServer for email verification codes, uses MySQL/Redis
for account data, and calls StatusServer over gRPC after a successful password
check to obtain the ChatServer endpoint and login token.

Main flow:
- `/get_varifycode` calls `VerifyGrpcClient`.
- `/user_register` validates the Redis verification code and writes the user to MySQL.
- `/reset_pwd` validates the Redis verification code and updates MySQL.
- `/user_login` checks MySQL credentials, then calls `StatusGrpcClient::GetChatServer(uid)`.

## Build

Use Visual Studio 2022 or MSBuild:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'D:\Organized_Files\Projects\Active_Development\CPP_Boost\GateServer\GateServer.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /m
```

Debug x64 is the maintained configuration. The project uses C++17, `/utf-8`,
Boost.Beast/Asio, jsoncpp, redis++, MySQL Connector/C++, protobuf, and gRPC.

## Runtime Config

`config.ini` is read from the process working directory:

```ini
[GateServer]
Port = 8080

[VarifyServer]
Host = 127.0.0.1
Port = 50051

[StatusServer]
Host = 127.0.0.1
Port = 50052
```

The StatusServer section must match the gRPC address exposed by the sibling
`StatusServer` project.

## StatusServer Contract

GateServer currently consumes the generated `StatusService.GetChatServer` RPC:

```proto
message GetChatServerReq {
  int32 uid = 1;
}

message GetChatServerRsp {
  int32 error = 1;
  string host = 2;
  string port = 3;
  string token = 4;
}
```

`StatusGrpcClient::GetChatServer(uid)` expects:
- `error == 0` for success.
- `host` and `port` to point to a ChatServer.
- `token` to be returned to the client and later used by ChatServer login logic.

Important: `message.proto` mentions a `Login` RPC, but the checked-in generated
`message.pb.*` and `message.grpc.pb.*` files currently only expose
`GetChatServer`. Regenerate the protobuf files before relying on any new RPC.

## Code Structure

- `GateServer.cpp`: process entry, reads config, starts HTTP server, initializes Redis/MySQL.
- `CServer.*`: Boost.Beast TCP acceptor for HTTP connections.
- `HttpConnection.*`: parses HTTP requests and dispatches to `LogicSystem`.
- `LogicSystem.*`: registers URL handlers and contains account/login flow.
- `VerifyGrpcClient.*`: gRPC client pool for VerifyServer.
- `StatusGrpcClient.*`: gRPC client pool for StatusServer.
- `MysqlMgr.*` / `MysqlDao.*`: account persistence.
- `RedisMgr.*`: verification-code cache and Redis helpers.
- `ConfigMgr.*`: INI config loader.
- `message.proto` plus generated `message.pb.*` / `message.grpc.pb.*`: gRPC schema.

## Change Guidelines

1. Keep slow work out of low-level networking code; put business behavior in
   `LogicSystem` or lower service wrappers.
2. When changing gRPC contracts, update `message.proto`, regenerate the `.pb`
   files, and update all sibling services that include them.
3. Keep `StatusGrpcClient` aligned with the StatusServer project. The config key
   names are `[StatusServer] Host` and `[StatusServer] Port`.
4. If adding Chinese comments, keep files encoded as UTF-8 and keep `/utf-8`
   enabled in the project.
5. After adding C++ files, include them in both `GateServer.vcxproj` and
   `GateServer.vcxproj.filters`.

## Verification

At minimum, run the Debug x64 MSBuild command above. A successful build should
finish with 0 errors. Warnings from third-party gRPC/protobuf headers may appear
depending on the local library version.
