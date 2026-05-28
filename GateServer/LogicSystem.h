#pragma once
#include "const.h"

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandle;

class LogicSystem : public SingleTon<LogicSystem>
{
    friend class SingleTon<LogicSystem>;
public:
    ~LogicSystem() {}
    bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
    void RegGet(std::string, HttpHandle handler);
    void RegPost(std::string url, HttpHandle handler);
    bool HandlePost(std::string path, std::shared_ptr<HttpConnection>);

private:
    LogicSystem();
    std::map<std::string, HttpHandle> _post_handlers;
    std::map<std::string, HttpHandle> _get_handlers;
};
