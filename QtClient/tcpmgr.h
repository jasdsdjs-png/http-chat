#ifndef TCPMGR_H
#define TCPMGR_H
#include <QTcpSocket>
#include <QMap>
#include "singleton.h"
#include "global.h"



class TcpMgr:public QObject, public SingleTon<TcpMgr>,
               public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT
    friend class SingleTon<TcpMgr>;
public:
    TcpMgr();
private:
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data);

    // 负责实际 TCP 连接、收包和发包的 Qt socket。
    QTcpSocket _socket;

    // 记录当前连接的服务器地址，便于调试或后续重连使用。
    QString _host;
    uint16_t _port;

    // 收包缓存。TCP 是字节流，readyRead 每次拿到的数据不一定刚好是一整包，
    // 所以需要把多次到达的数据累积起来再按协议解析。
    QByteArray _buffer;

    // true 表示已经读到包头，但包体还没收完整；下一次 readyRead 继续等待包体。
    bool _b_recv_pending;

    // 当前正在解析的数据包头：协议格式为 2 字节消息 ID + 2 字节消息体长度。
    quint16 _message_id;
    quint16 _message_len;

    QMap<ReqId, std::function<void(ReqId, int, QByteArray)>> _handlers;
public slots:
    // 根据服务器信息发起 TCP 连接。
    void slot_tcp_connect(ServerInfo);

    // 按自定义协议发送数据：消息 ID(2 字节) + 消息体长度(2 字节) + UTF-8 消息体。
    void slot_send_data(ReqId reqId, QString data);
signals:
    // TCP 连接成功或失败时通知界面/业务层。
    void sig_con_success(bool bsuccess);

    // 外部通过信号触发发送，统一落到 slot_send_data 执行实际写入。
    void sig_send_data(ReqId reqId, QString data);

    // ChatServer 登录响应失败时通知登录页。
    void sig_login_failed(int err);

    // ChatServer 登录响应成功后通知外层切换到聊天界面，聊天界面在后续教程中实现。
    void sig_swich_chatdlg();
};

#endif // TCPMGR_H
