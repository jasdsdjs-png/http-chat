#include "usermgr.h"

UserMgr::UserMgr()
{
}

UserMgr::~UserMgr()
{
}

void UserMgr::SetName(QString name)
{
    _name = name;
}

void UserMgr::SetUid(int uid)
{
    _uid = uid;
}

void UserMgr::SetToken(QString token)
{
    _token = token;
}

QString UserMgr::GetName() const
{
    return _name;
}

int UserMgr::GetUid() const
{
    return _uid;
}

QString UserMgr::GetToken() const
{
    return _token;
}
