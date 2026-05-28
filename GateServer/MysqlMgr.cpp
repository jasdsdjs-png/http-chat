#include "MysqlMgr.h"
#include "MysqlMgr.h"


MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
    return _dao.RegUser(name, email, pwd);
}

int MysqlMgr::ResetPwd(const std::string& name, const std::string& email, const std::string& pwd)
{
    return _dao.ResetPwd(name, email, pwd);
}

bool MysqlMgr::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo)
{
    return _dao.CheckPwd(name, pwd, userInfo);
}

MysqlMgr::MysqlMgr() {
}
