#include "tcpmgr.h"
#include "usermgr.h"

#include <QJsonDocument>
#include <QJsonObject>

TcpMgr::TcpMgr():_host(""),_port(0),_b_recv_pending(false),_message_id(0),_message_len(0)
{
    initHandlers();

    // connected 只表示 TCP 三次握手已经完成，可以开始发送业务数据。
    QObject::connect(&_socket, &QTcpSocket::connected, [&]() {
        qDebug() << "Connected to server!";
        emit sig_con_success(true);
    });

    // readyRead 可能收到半包、整包或多个包，因此这里不直接假设 readAll 是一条完整消息。
    // 数据会先追加到 _buffer，再按照 “消息 ID + 消息长度 + 消息体” 的协议循环拆包。
    QObject::connect(&_socket, &QTcpSocket::readyRead, [&]() {
        _buffer.append(_socket.readAll());

        forever {   //forever相当于while(1)
            // 先解析固定长度包头。若上一轮已经拿到包头但包体不完整，则跳过本段继续等包体。
            if(!_b_recv_pending){
                if (_buffer.size() < static_cast<int>(sizeof(quint16) * 2)) {
                    return;
                }

                // 每轮都只从当前缓存的前 4 字节读取包头，避免粘包时复用旧 QDataStream 读指针。
                //从 _buffer 的最左边，也就是开头，复制前 4 个字节出来
                QByteArray header = _buffer.left(sizeof(quint16) * 2);
                QDataStream stream(&header, QIODevice::ReadOnly);
                stream.setVersion(QDataStream::Qt_5_0);
                stream.setByteOrder(QDataStream::BigEndian);//设置数据为大端序
                stream >> _message_id >> _message_len;

                // 包头已成功解析，从缓存中移除这 4 个字节，剩余部分只保留包体和后续粘包数据。
                //表示从下标4开始截取字节
                _buffer = _buffer.mid(sizeof(quint16) * 2);

                qDebug() << "Message ID:" << _message_id << ", Length:" << _message_len;

            }

            // 包体还没收完整时先退出，等下一次 readyRead 追加数据后继续解析同一个包。
            if(_buffer.size() < _message_len){
                _b_recv_pending = true;
                return;
            }

            _b_recv_pending = false;

            // 截取当前消息体，并把已消费的数据从缓存中移除；缓存尾部可能还包含下一包数据。
            QByteArray messageBody = _buffer.mid(0, _message_len);
            qDebug() << "receive body msg is " << messageBody ;

            _buffer = _buffer.mid(_message_len);
            handleMsg(static_cast<ReqId>(_message_id), _message_len, messageBody);
        }

    });
    //收到数据循环解析，因为tcp是字节流，无法保证一次就完整收到数据
    //如果包头不够，等下一次tcp
    //包头够但包体不够，读出包头，从缓存删除包头，等包体完整再解析包体
    //包体够了读取包体，然后从缓存中删除

    // 统一处理连接错误，并把连接失败结果通过 sig_con_success(false) 通知出去。
    QObject::connect(&_socket, &QTcpSocket::errorOccurred,
                     [&](QTcpSocket::SocketError socketError) {
                         qDebug() << "Error:" << _socket.errorString() ;
                         switch (socketError) {
                         case QTcpSocket::ConnectionRefusedError:
                             qDebug() << "Connection Refused!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::RemoteHostClosedError:
                             qDebug() << "Remote Host Closed Connection!";
                             break;
                         case QTcpSocket::HostNotFoundError:
                             qDebug() << "Host Not Found!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::SocketTimeoutError:
                             qDebug() << "Connection Timeout!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::NetworkError:
                             qDebug() << "Network Error!";
                             break;
                         default:
                             qDebug() << "Other Error!";
                             break;
                         }
                     });

    // 服务端主动断开或本地断开连接时都会触发，当前只打印日志。
    QObject::connect(&_socket, &QTcpSocket::disconnected, [&]() {
        qDebug() << "Disconnected from server.";
    });

    // 对外暴露 sig_send_data 信号，调用方不用直接访问 QTcpSocket。
    QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
}

void TcpMgr::initHandlers()
{
    _handlers.insert(ReqId::ID_CHAT_LOGIN_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "handle id is " << id << " data is " << data;

        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qDebug() << "Login Failed, err is Json Parse Err" << ErrorCodes::ERR_JSON;
            emit sig_login_failed(ErrorCodes::ERR_JSON);
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) {
            qDebug() << "Login Failed, err is Json Parse Err" << ErrorCodes::ERR_JSON;
            emit sig_login_failed(ErrorCodes::ERR_JSON);
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "Login Failed, err is " << err;
            emit sig_login_failed(err);
            return;
        }

        UserMgr::GetInstance()->SetUid(jsonObj["uid"].toInt());
        UserMgr::GetInstance()->SetName(jsonObj["name"].toString());
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());
        emit sig_swich_chatdlg();
    });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto find_iter = _handlers.find(id);
    if (find_iter == _handlers.end()) {
        qDebug() << "not found id [" << id << "] to handle";
        return;
    }

    find_iter.value()(id, len, data);
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug()<< "receive tcp connect signal";
    qDebug() << "Connecting to server...";
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _buffer.clear();
    _b_recv_pending = false;
    _message_id = 0;
    _message_len = 0;
    if (_socket.state() != QAbstractSocket::UnconnectedState) {
        _socket.abort();
    }
    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QString data)
{
    uint16_t id = reqId;

    // 业务字符串统一转成 UTF-8 字节发送，避免中文等多字节字符在网络上传输时丢失信息。
    QByteArray dataBytes = data.toUtf8();

    // 包体长度必须使用实际字节数，而不是 QString 的字符数；否则中文内容会导致接收端拆包错位。
    if (dataBytes.size() > 0xFFFF) { //包的大小不能超过len的表示的最大长度
        qWarning() << "TcpMgr send failed: message body is larger than quint16 length field";
        return;
    }
    quint16 len = static_cast<quint16>(dataBytes.size());

    // block 是最终写入 socket 的完整数据包：包头 + 包体。
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 使用大端序，和接收端读取包头时保持一致。
    out.setByteOrder(QDataStream::BigEndian);

    // 写入固定长度包头：消息 ID 和 UTF-8 包体长度。
    out << id << len;

    // 追加真实包体字节。
    block.append(dataBytes);

    // 将完整数据包交给 Qt 的异步 socket 发送。
    _socket.write(block);
}
