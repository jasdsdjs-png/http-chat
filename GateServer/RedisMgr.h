#pragma once
#include "const.h"
#include <memory>
#include <string>

class RedisMgr : public SingleTon<RedisMgr>,
    public std::enable_shared_from_this<RedisMgr>
{
    friend class SingleTon<RedisMgr>;
public:
    ~RedisMgr();
    bool Connect(const std::string& host, int port);
    bool Auth(const std::string& password);
    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    bool LPush(const std::string& key, const std::string& value);
    bool LPop(const std::string& key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string& key, const std::string& hkey);
    bool Del(const std::string& key);
    bool ExistsKey(const std::string& key);
    long long TTL(const std::string& key);
    void Close();
private:
    RedisMgr();
    std::unique_ptr<sw::redis::Redis> _redis;
    std::string _host;
    int _port;
    std::string _password;
};
