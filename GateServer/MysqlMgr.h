#pragma once
#include "const.h"
#include "MysqlDao.h"
class MysqlMgr : public SingleTon<MysqlMgr>
{
    friend class SingleTon<MysqlMgr>;
public:
    ~MysqlMgr();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    int ResetPwd(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
private:
    MysqlMgr();
    MysqlDao  _dao;
};
