#ifndef HTTPMGR_H
#define HTTPMGR_H
#include<QString>
#include<QUrl>
#include<QObject>
#include<QNetworkAccessManager> //网络管理器，发送http请求的核心类
#include<QJsonObject>  //json对象
#include<QJsonDocument>
#include"singleton.h"

//CRTP
//CRTP（Curiously Recurring Template Pattern，奇异递归模板模式）是 C++ 模板元编程中一个
//经典且强大的惯用法。它的核心思想是：派生类将自身作为模板参数传递给基类模板，从而让基类在编译期就能"知道"派生类的类型。
//构造函数为私有且基类设置友元：限制构造对象的唯一性
//析构函数为公共：配合 std::shared_ptr 的引用计数销毁
// std::shared_ptr 默认的删除器无法在类外调用
//如果自定义了share_ptr的删除器，析构函数可以设为私有
class HttpMgr:public QObject,public SingleTon<HttpMgr>,public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT
public:
    ~HttpMgr();
     void PostHttpReq(QUrl url,QJsonObject json,ReqId req_id,Modules mod);
private:

    friend class SingleTon<HttpMgr>;
    HttpMgr();
    QNetworkAccessManager _manager;

private slots:
    void slot_http_finish(ReqId req_id,QString res,ErrorCodes error,Modules mod);

signals:
    void sig_http_finish(ReqId req_id,QString res,ErrorCodes error,Modules mod);//当http发送请求后，用于通知其他模块
    void sig_reg_mod_finish(ReqId req_id,QString res,ErrorCodes error);
    void sig_reset_mod_finish(ReqId req_id,QString res,ErrorCodes error);
    void sig_login_mod_finish(ReqId id, QString res, ErrorCodes err);
};

#endif // HTTPMGR_H
