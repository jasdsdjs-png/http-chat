#pragma once
#include"const.h"
#include"HttpConnection.h"


class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc, unsigned short& port);
	void Start();
private:
	tcp::acceptor _acceptor;//接收套接字，listen
	net::io_context& _ioc; //事件循环，相当于while(1) { epoll_wait(...); }
	tcp::socket _socket; //只用于临时连接，连接后移动赋值到HttpConnection中的socket

};
