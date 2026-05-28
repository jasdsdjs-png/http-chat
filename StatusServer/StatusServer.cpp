#include <grpcpp/grpcpp.h>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "StatusServerCore.h"
#include "message.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

namespace {

struct StatusServerConfig {
    std::string host = "0.0.0.0";
    std::string port = "50052";
    std::vector<ChatServerEndpoint> chat_servers;
};

std::vector<ChatServerEndpoint> ParseChatServers(const std::string& value) {
    std::vector<ChatServerEndpoint> servers;
    size_t begin = 0;

    while (begin < value.size()) {
        const size_t end = value.find(',', begin);
        const std::string item = value.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        const size_t split = item.find(':');

        if (split != std::string::npos && split > 0 && split + 1 < item.size()) {
            servers.push_back(ChatServerEndpoint{item.substr(0, split), item.substr(split + 1)});
        }

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    return servers;
}

StatusServerConfig LoadConfig() {
    StatusServerConfig config;
    boost::property_tree::ptree pt;
    boost::property_tree::read_ini("config.ini", pt);

    // StatusServer is the gRPC endpoint consumed by GateServer::StatusGrpcClient.
    config.host = pt.get<std::string>("StatusServer.Host", config.host);
    config.port = pt.get<std::string>("StatusServer.Port", config.port);

    // ChatServers.List uses host:port entries because GateServer returns these directly to clients.
    config.chat_servers = ParseChatServers(pt.get<std::string>("ChatServers.List", "127.0.0.1:10087"));
    if (config.chat_servers.empty()) {
        config.chat_servers.push_back(ChatServerEndpoint{"127.0.0.1", "10087"});
    }

    return config;
}

class StatusServiceImpl final : public StatusService::Service {
public:
    explicit StatusServiceImpl(std::vector<ChatServerEndpoint> chat_servers)
        : core_(std::move(chat_servers)) {}

    Status GetChatServer(ServerContext*, const GetChatServerReq* request, GetChatServerRsp* reply) override {
        const auto allocation = core_.GetChatServer(request->uid());
        reply->set_error(allocation.error);
        reply->set_host(allocation.host);
        reply->set_port(allocation.port);
        reply->set_token(allocation.token);
        return Status::OK;
    }

    Status Login(ServerContext*, const LoginReq* request, LoginRsp* reply) override {
        const auto result = core_.Login(request->uid(), request->token());
        reply->set_error(result.error);
        reply->set_uid(result.uid);
        reply->set_token(result.token);
        return Status::OK;
    }

private:
    StatusServiceCore core_;
};

}  // namespace

int main() {
    try {
        auto config = LoadConfig();
        const std::string server_address = config.host + ":" + config.port;

        StatusServiceImpl service(std::move(config.chat_servers));
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<Server> server(builder.BuildAndStart());
        if (!server) {
            std::cerr << "StatusServer failed to listen on " << server_address << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "StatusServer listening on " << server_address << std::endl;
        server->Wait();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "StatusServer error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
