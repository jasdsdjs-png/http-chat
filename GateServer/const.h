#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include "SingleTon.h"
#include <functional>
#include <map>
#include <unordered_map>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <sw/redis++/redis++.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

enum ErrorCodes {
    Success = 0,
    Error_Json = 1001,      // JSON parse failed.
    RPCFailed = 1002,       // RPC call failed.
    VarifyExpired = 1003,   // Verification code expired.
    VarifyCodeErr = 1004,   // Verification code mismatch.
    UserExist = 1005,       // User already exists.
    PasswdErr = 1006,       // Passwords do not match.
    EmailNotMatch = 1007,   // Email does not match.
    PasswdUpFailed = 1008,  // Password update failed.
    PasswdInvalid = 1009,   // Invalid password format.
    Error_Param = 1010,     // Invalid parameter.
    UserNotExist = 1011,    // User does not exist.
};

struct UserInfo {
    int uid = 0;
    std::string name;
    std::string email;
    std::string pwd;
};
