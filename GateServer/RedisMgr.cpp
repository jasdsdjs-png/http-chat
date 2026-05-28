#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisMgr::RedisMgr() : _port(6379) {
    auto& cfg = ConfigMgr::Inst();
    _host = cfg["Redis"]["Host"];
    _password = cfg["Redis"]["Passwd"];
    try {
        _port = std::stoi(cfg["Redis"]["Port"]);
    } catch (...) {
        _port = 6380;
    }
    std::cout << "RedisMgr init: " << _host << ":" << _port << std::endl;

    try {
        sw::redis::ConnectionOptions conn_opts;
        conn_opts.host = _host;
        conn_opts.port = _port;
        conn_opts.password = _password;
        conn_opts.socket_timeout = std::chrono::milliseconds(2000);
        conn_opts.connect_timeout = std::chrono::milliseconds(2000);

        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 5;

        _redis = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);
        _redis->ping();
        std::cout << "Redis connection pool created, connected to " << _host << ":" << _port << std::endl;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Redis init error: " << e.what() << std::endl;
        _redis.reset();
    }
    catch (const std::exception& e) {
        std::cout << "Redis init error: " << e.what() << std::endl;
        _redis.reset();
    }
}
RedisMgr::~RedisMgr() {}

bool RedisMgr::Connect(const std::string& host, int port)
{
    _host = host;
    _port = port;
    std::cout << "Redis connect info stored: " << host << ":" << port << std::endl;
    return true;
}

bool RedisMgr::Auth(const std::string& password)
{
    _password = password;
    try {
        sw::redis::ConnectionOptions conn_opts;
        conn_opts.host = _host;
        conn_opts.port = _port;
        conn_opts.password = _password;
        conn_opts.socket_timeout = std::chrono::milliseconds(2000);
        conn_opts.connect_timeout = std::chrono::milliseconds(2000);

        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 5;

        _redis = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);
        // Test connection by pinging
        _redis->ping();
        std::cout << "Redis connection pool created, auth success!" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Redis auth/connect error: " << e.what() << std::endl;
        _redis.reset();
        return false;
    }
    catch (const std::exception& e) {
        std::cout << "Redis auth/connect error: " << e.what() << std::endl;
        _redis.reset();
        return false;
    }
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
    if (!_redis) return false;
    try {
        auto val = _redis->get(key);
        if (!val) {
            std::cout << "[ GET " << key << " ] failed: key not found" << std::endl;
            return false;
        }
        value = *val;
        std::cout << "Succeed to execute command [ GET " << key << " ]" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "[ GET " << key << " ] failed: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::Set(const std::string& key, const std::string& value)
{
    if (!_redis) return false;
    try {
        _redis->set(key, value);
        std::cout << "Execut command [ SET " << key << " " << value << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ SET " << key << " " << value << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
    if (!_redis) return false;
    try {
        long long len = _redis->lpush(key, value);
        if (len <= 0) {
            std::cout << "Execut command [ LPUSH " << key << " " << value << " ] failure !" << std::endl;
            return false;
        }
        std::cout << "Execut command [ LPUSH " << key << " " << value << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ LPUSH " << key << " " << value << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::LPop(const std::string& key, std::string& value)
{
    if (!_redis) return false;
    try {
        auto val = _redis->lpop(key);
        if (!val) {
            std::cout << "Execut command [ LPOP " << key << " ] failure !" << std::endl;
            return false;
        }
        value = *val;
        std::cout << "Execut command [ LPOP " << key << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ LPOP " << key << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::RPush(const std::string& key, const std::string& value)
{
    if (!_redis) return false;
    try {
        long long len = _redis->rpush(key, value);
        if (len <= 0) {
            std::cout << "Execut command [ RPUSH " << key << " " << value << " ] failure !" << std::endl;
            return false;
        }
        std::cout << "Execut command [ RPUSH " << key << " " << value << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ RPUSH " << key << " " << value << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::RPop(const std::string& key, std::string& value)
{
    if (!_redis) return false;
    try {
        auto val = _redis->rpop(key);
        if (!val) {
            std::cout << "Execut command [ RPOP " << key << " ] failure !" << std::endl;
            return false;
        }
        value = *val;
        std::cout << "Execut command [ RPOP " << key << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ RPOP " << key << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value)
{
    if (!_redis) return false;
    try {
        _redis->hset(key, hkey, value);
        std::cout << "Execut command [ HSet " << key << " " << hkey << " " << value << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ HSet " << key << " " << hkey << " " << value << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
    if (!_redis) return false;
    try {
        _redis->hset(key, hkey, std::string(hvalue, hvaluelen));
        std::cout << "Execut command [ HSet " << key << " " << hkey << " ... ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ HSet " << key << " " << hkey << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
    if (!_redis) return "";
    try {
        auto val = _redis->hget(key, hkey);
        if (!val) {
            std::cout << "Execut command [ HGet " << key << " " << hkey << " ] failure !" << std::endl;
            return "";
        }
        std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success !" << std::endl;
        return *val;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ HGet " << key << " " << hkey << " ] failure: " << e.what() << std::endl;
        return "";
    }
}

bool RedisMgr::Del(const std::string& key)
{
    if (!_redis) return false;
    try {
        long long count = _redis->del(key);
        if (count == 0) {
            std::cout << "Execut command [ Del " << key << " ] failure: key not found" << std::endl;
            return false;
        }
        std::cout << "Execut command [ Del " << key << " ] success !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "Execut command [ Del " << key << " ] failure: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMgr::ExistsKey(const std::string& key)
{
    if (!_redis) return false;
    try {
        long long count = _redis->exists(key);
        if (count == 0) {
            std::cout << "Not Found [ Key " << key << " ] !" << std::endl;
            return false;
        }
        std::cout << "Found [ Key " << key << " ] exists !" << std::endl;
        return true;
    }
    catch (const sw::redis::Error& e) {
        std::cout << "ExistsKey [ " << key << " ] error: " << e.what() << std::endl;
        return false;
    }
}

void RedisMgr::Close()
{
    _redis.reset();
    std::cout << "Redis connection pool closed." << std::endl;
}

long long RedisMgr::TTL(const std::string& key)
{
    if (!_redis) return -2;
    try {
        return _redis->ttl(key);
    }
    catch (const sw::redis::Error& e) {
        std::cout << "[ TTL " << key << " ] failed: " << e.what() << std::endl;
        return -2;
    }
}
