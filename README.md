# http-chat

HTTP + TCP chat demo, organized as one repository for the Qt client and backend services.

## Directory Layout

- `QtClient/`: Qt desktop client.
- `GateServer/`: HTTP gateway for register, login, password reset, and chat-server allocation.
- `ChatServer/`: TCP chat server. It verifies `uid/token` with `StatusServer` before accepting chat login.
- `StatusServer/`: gRPC service for chat-server allocation and token verification.
- `VarifyServer/`: verification-code service.

The project is a monorepo. The Qt client and backend services are normal folders in this repository, not git submodules.

## External Dependencies

Third-party libraries are intentionally not committed.

The C++ services expect these local dependencies:

- Boost headers/libs, referenced by the Visual Studio projects as `..\boost_`.
- gRPC / protobuf, currently referenced by absolute local paths such as `D:\grpc.file`.
- JsonCpp, Redis++, hiredis, OpenSSL, zlib, and MySQL Connector/C++.
- `GateServer` and `ChatServer` currently reference MySQL Connector/C++ under `GateServer\mysql` and the runtime DLL `GateServer\mysqlcppconn-10-vs14.dll`; place those files locally before building, or adjust the project include/library paths.

The Qt client expects Qt 6 with MinGW, for example:

```powershell
cd QtClient
D:\Qt\6.11.0\mingw_64\bin\qmake.exe project.pro
$env:Path = 'D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.11.0\mingw_64\bin;' + $env:Path
mingw32-make -j4
```

## Login Flow

1. The Qt client logs in through `GateServer`.
2. `GateServer` verifies the user and asks `StatusServer` for a ChatServer endpoint and token.
3. The Qt client connects to `ChatServer` by TCP and sends `ID_CHAT_LOGIN = 1005` with `uid/token`.
4. `ChatServer` calls `StatusServer.Login` to verify the token.
5. `ChatServer` returns `ID_CHAT_LOGIN_RSP = 1006`; the Qt client stores `uid/name/token` in `UserMgr`.
