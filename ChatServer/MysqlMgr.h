#pragma once

#include "MysqlDao.h"
#include "Singleton.h"

class MysqlMgr : public Singleton<MysqlMgr> {
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr();

    std::shared_ptr<UserInfo> GetUser(int uid);

private:
    MysqlMgr();

    MysqlDao dao_;
};

