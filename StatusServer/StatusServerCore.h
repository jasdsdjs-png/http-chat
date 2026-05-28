#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// GateServer expects the same error numbers that are defined in its const.h.
// Keeping the values here avoids including GateServer business headers in this service.
enum StatusErrorCodes {
    StatusSuccess = 0,
    StatusRPCFailed = 1002,
    StatusErrorParam = 1010,
    StatusUidInvalid = 1012,
    StatusTokenInvalid = 1013,
};

struct ChatServerEndpoint {
    std::string host;
    std::string port;
};

struct ChatServerAllocation {
    int error = StatusSuccess;
    std::string host;
    std::string port;
    std::string token;
};

struct LoginResult {
    int error = StatusSuccess;
    int uid = 0;
    std::string token;
};

class StatusServiceCore {
public:
    explicit StatusServiceCore(std::vector<ChatServerEndpoint> servers)
        : servers_(std::move(servers)), next_index_(0), rng_(std::random_device{}()) {}

    ChatServerAllocation GetChatServer(int uid) {
        ChatServerAllocation result;
        if (uid <= 0 || servers_.empty()) {
            result.error = StatusErrorParam;
            return result;
        }

        const size_t index = next_index_.fetch_add(1, std::memory_order_relaxed) % servers_.size();
        const auto& server = servers_[index];

        result.host = server.host;
        result.port = server.port;
        result.token = GenerateToken(uid);

        {
            std::lock_guard<std::mutex> lock(tokens_mutex_);
            tokens_[uid] = result.token;
        }

        return result;
    }

    LoginResult Login(int uid, const std::string& token) const {
        LoginResult result;
        result.uid = uid;
        result.token = token;

        std::lock_guard<std::mutex> lock(tokens_mutex_);
        auto iter = tokens_.find(uid);
        if (uid <= 0 || token.empty() || iter == tokens_.end()) {
            result.error = StatusUidInvalid;
        } else if (iter->second != token) {
            result.error = StatusTokenInvalid;
        }

        return result;
    }

private:
    std::string GenerateToken(int uid) {
        std::uniform_int_distribution<std::uint64_t> dist;
        std::ostringstream oss;
        oss << uid << "-" << dist(rng_) << "-" << dist(rng_);
        return oss.str();
    }

    std::vector<ChatServerEndpoint> servers_;
    std::atomic<size_t> next_index_;
    mutable std::mutex tokens_mutex_;
    std::unordered_map<int, std::string> tokens_;
    std::mt19937_64 rng_;
};
