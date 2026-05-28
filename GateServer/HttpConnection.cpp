#include "HttpConnection.h"

#include <iostream>

#include "LogicSystem.h"

HttpConnection::HttpConnection(tcp::socket socket)
    : _socket(std::move(socket)),
      deadline_(_socket.get_executor(), std::chrono::seconds(60))
{
}

void HttpConnection::Start() {
    auto self = shared_from_this();
    CheckDeadLine();
    http::async_read(_socket, _buffer, _request,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            try {
                if (ec) {
                    std::cout << "http read err: " << ec.message() << std::endl;
                    return;
                }
                boost::ignore_unused(bytes_transferred);
                self->HandleReq();
            }
            catch (std::exception& exp) {
                std::cout << "exception: " << exp.what() << std::endl;
            }
        });
}

std::string HttpConnection::ParseUrl(const std::string& target) {
    auto pos = target.find('?');
    if (pos == std::string::npos)
        return target;
    ParseQueryString(target.substr(pos + 1));
    return target.substr(0, pos);
}

void HttpConnection::ParseQueryString(const std::string& query) {
    if (query.empty())
        return;

    std::string key, value;
    bool is_key = true;
    for (size_t i = 0; i <= query.size(); ++i) {
        char ch = (i < query.size()) ? query[i] : '&';
        if (ch == '&') {
            if (!key.empty()) {
                _query_params[UrlDecode(key)] = UrlDecode(value);
                key.clear();
                value.clear();
                is_key = true;
            }
        } else if (ch == '=' && is_key) {
            is_key = false;
        } else if (is_key) {
            key += ch;
        } else {
            value += ch;
        }
    }
}

std::string HttpConnection::GetQueryParam(const std::string& key) const {
    auto it = _query_params.find(key);
    if (it != _query_params.end())
        return it->second;
    return std::string();
}

void HttpConnection::HandleReq() {
    _response.version(_request.version());
    _response.keep_alive(false);

    // Dispatch GET requests to LogicSystem.
    if (_request.method() == http::verb::get) {
        std::string path = ParseUrl(std::string(_request.target()));
        bool success = LogicSystem::GetInstance()->HandleGet(
            path, shared_from_this());
        if (!success) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/json");
            Json::Value root;
            root["error"] = ErrorCodes::Error_Param;
            root["msg"] = "url not found";
            beast::ostream(_response.body()) << root.toStyledString();
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }

    // Dispatch POST requests to LogicSystem.
    if (_request.method() == http::verb::post) {
        std::string path = ParseUrl(std::string(_request.target()));
        bool success = LogicSystem::GetInstance()->HandlePost(path, shared_from_this());
        if (!success) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/json");
            Json::Value root;
            root["error"] = ErrorCodes::Error_Param;
            root["msg"] = "url not found";
            beast::ostream(_response.body()) << root.toStyledString();
            WriteResponse();
            return;
        }

        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }

    // Reject unsupported HTTP methods.
    _response.result(http::status::method_not_allowed);
    _response.set(http::field::content_type, "text/json");
    Json::Value root;
    root["error"] = ErrorCodes::Error_Param;
    root["msg"] = "method not allowed";
    beast::ostream(_response.body()) << root.toStyledString();
    WriteResponse();
}

void HttpConnection::WriteResponse() {
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(_socket, _response,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);
            self->deadline_.cancel();
        });
}

void HttpConnection::CheckDeadLine() {
    auto self = shared_from_this();
    deadline_.async_wait([self](beast::error_code ec) {
        if (!ec)
            self->_socket.close(ec);
    });
}

unsigned char HttpConnection::FromHex(unsigned char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return 0;
}

unsigned char HttpConnection::ToHex(unsigned char x) {
    return x > 9 ? x + 'A' - 10 : x + '0';
}

std::string HttpConnection::UrlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            unsigned char high = FromHex((unsigned char)str[i + 1]);
            unsigned char low = FromHex((unsigned char)str[i + 2]);
            result += (char)((high << 4) | low);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string HttpConnection::UrlEncode(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 3);
    for (unsigned char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            result += '%';
            result += ToHex(c >> 4);
            result += ToHex(c & 0x0F);
        }
    }
    return result;
}
