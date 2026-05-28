# QtClient - Qt 桌面客户端（登录/注册/重置密码）

## 项目概述

这是一个基于 Qt6 Widgets 的 C++ 桌面客户端，包含登录、注册、重置密码三个模块。客户端通过 HTTP JSON API 与后端 GateServer 通信，使用 `HttpMgr` 单例统一管理异步网络请求，并通过 QSS 维护界面样式。

## 技术栈

| 技术 | 说明 |
|------|------|
| Qt | 6.11，widgets + network |
| C++ | C++17 |
| 构建 | qmake / MinGW |
| 网络 | QNetworkAccessManager |
| 数据 | QJsonObject / QJsonDocument |
| 样式 | QSS |

## 项目结构

```
QtClient/
|-- main.cpp
|-- mainwindow.cpp/h
|-- logindialog.cpp/h
|-- registerdialog.cpp/h
|-- resetpassworddialog.cpp/h
|-- httpmgr.cpp/h
|-- clickedlabel.cpp/h
|-- singleton.h
|-- global.cpp/h
|-- config.ini
|-- stytle/systle.qss
|-- res.qrc
|-- icon/
|-- *.ui
`-- project.pro
```

## 运行逻辑

`main.cpp` 加载 QSS、读取 `config.ini`，拼出 `gate_url_prefix` 后显示 `MainWindow`。

`MainWindow` 默认显示 `LoginDialog`。登录页点击注册时发出 `switchRegister()`，主窗口进入 `slotSwitchReg()` 并切换到 `RegisterDialog`。点击忘记密码时发出 `switchForgetPassword()`，主窗口进入 `slotSwitchReset()` 并切换到 `ResetPasswordDialog`。注册或重置完成、取消、返回登录时，对话框发出 `sigSwitchLogin()`，主窗口进入 `slotSwitchLogin()` 回到登录页。

网络请求统一通过：

```cpp
HttpMgr::GetInstance()->PostHttpReq(url, json, req_id, module);
```

HTTP 完成后先触发 `sig_http_finish`，再由 `slot_http_finish()` 根据模块分发：

- `REGISTERMOD` -> `sig_reg_mod_finish`
- `RESETPWDMOD` -> `sig_reset_mod_finish`
- `LOGINMOD` -> `sig_login_mod_finish`

各页面收到模块信号后解析 JSON，并通过 `_handlers` 按 `ReqId` 调用对应处理逻辑。

## API 端点

| 端点 | 功能 | 请求字段 |
|------|------|----------|
| `/get_varifycode` | 获取验证码 | `email` |
| `/user_register` | 注册 | `user`, `email`, `passwd`, `confirm`, `varifycode` |
| `/user_login` | 登录 | `user`, `passwd` |
| `/reset_pwd` | 重置密码 | `user`, `email`, `passwd`, `confirm`, `varifycode` |

## 当前修复状态

### 已修复

1. `logindialog.cpp` 末尾多余字符 `v` 已移除。
2. `LoginDialog` 已补充 `_handlers` 成员和 `showTip()`。
3. `LoginDialog` 构造函数已调用 `initHttpHandlers()`。
4. 登录页已补充 `error_label`，用于显示登录错误提示。
5. 登录回包分发已先检查 handler 是否存在，避免未知 `ReqId` 直接调用空处理器。
6. `HttpMgr::sig_login_mod_finish` 已统一为 3 个参数：`ReqId, QString, ErrorCodes`。
7. 已添加 `.gitignore`，排除 `build/`、Qt Creator 用户文件、临时图片和二进制产物。

### 仍需注意

1. `RegisterDialog` 和 `ResetPasswordDialog` 中同时存在 `_countdownTimer` 与 `_countdown_timer`，命名接近，后续可重命名降低误用风险。
2. 网络错误时 `HttpMgr` 发送的响应字符串为 `" "`；当前对话框会先判断错误码，不会继续解析 JSON，但后续可改成更明确的错误对象或空字符串。
3. 样式文件目录名当前为 `stytle/systle.qss`，这是资源文件中的实际拼写，修改目录名时需要同步 `res.qrc` 和 `main.cpp`。

## 构建验证

本地可用构建命令：

```powershell
$env:PATH='D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
New-Item -ItemType Directory -Force -Path build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug | Out-Null
Push-Location build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug
D:\Qt\6.11.0\mingw_64\bin\qmake.exe ..\..\QtClient\project.pro -spec win32-g++ "CONFIG+=debug" "CONFIG+=qml_debug"
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe
Pop-Location
```

## Git 基线

项目应在修复并验证通过后创建初始提交。之后若要回滚到干净基线，可使用：

```powershell
git reset --hard HEAD
git clean -fd
```

注意：`git clean -fd` 会删除未跟踪文件，使用前确认没有要保留的新文件。
