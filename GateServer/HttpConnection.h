#pragma once

#include "const.h"
#include <unordered_map>

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
    friend class LogicSystem;

public:
    explicit HttpConnection(tcp::socket socket);
    void Start();
    std::string GetQueryParam(const std::string& key) const;

private:
    void CheckDeadLine();
    void WriteResponse();
    void HandleReq();
    std::string ParseUrl(const std::string& target);
    void ParseQueryString(const std::string& query);
    static unsigned char FromHex(unsigned char ch);
    static unsigned char ToHex(unsigned char x);
    std::string UrlDecode(const std::string& str);
    std::string UrlEncode(const std::string& str);

    std::unordered_map<std::string, std::string> _query_params;
    tcp::socket _socket;
    beast::flat_buffer _buffer{ 8192 };
    http::request<http::dynamic_body> _request;
    http::response<http::dynamic_body> _response;
    net::steady_timer deadline_;
};
