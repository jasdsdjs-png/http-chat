#ifndef USERMGR_H
#define USERMGR_H

#include "singleton.h"

#include <QObject>
#include <QString>
#include <memory>

class UserMgr : public QObject, public SingleTon<UserMgr>,
                public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
    friend class SingleTon<UserMgr>;

public:
    ~UserMgr();

    void SetName(QString name);
    void SetUid(int uid);
    void SetToken(QString token);

    QString GetName() const;
    int GetUid() const;
    QString GetToken() const;

private:
    UserMgr();

    QString _name;
    QString _token;
    int _uid = 0;
};

#endif // USERMGR_H
