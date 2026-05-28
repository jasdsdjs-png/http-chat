#include "MysqlMgr.h"

MysqlMgr::MysqlMgr() {}

MysqlMgr::~MysqlMgr() {}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid) {
    return dao_.GetUser(uid);
}

