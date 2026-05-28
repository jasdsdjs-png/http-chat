#include <iostream>
#include <cassert>

#include "ConfigMgr.h"
#include "CServer.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "const.h"

int main()
{
    try {
        std::string gate_port_str = ConfigMgr::Inst()["GateServer"]["Port"];
        unsigned short port = static_cast<unsigned short>(atoi(gate_port_str.c_str()));
        if (port == 0) {
            port = 8080;
        }

        net::io_context ioc{ 1 };

        std::make_shared<CServer>(ioc, port)->Start();

        std::cout << "GateServer listening on port " << port << std::endl;

        // Pre-initialize Redis and MySQL connections.
        RedisMgr::GetInstance();
        MysqlMgr::GetInstance();

        ioc.run();
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
