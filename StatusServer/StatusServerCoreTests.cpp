#include "StatusServerCore.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(1);
    }
}

}  // namespace

int main() {
    StatusServiceCore core({
        ChatServerEndpoint{"127.0.0.1", "10087"},
        ChatServerEndpoint{"127.0.0.1", "10088"},
    });

    auto first = core.GetChatServer(42);
    Expect(first.error == 0, "first allocation should succeed");
    Expect(first.host == "127.0.0.1", "first allocation host");
    Expect(first.port == "10087", "first allocation uses first server");
    Expect(!first.token.empty(), "first allocation creates a token");

    auto second = core.GetChatServer(43);
    Expect(second.error == 0, "second allocation should succeed");
    Expect(second.port == "10088", "second allocation round-robins");

    auto login_ok = core.Login(42, first.token);
    Expect(login_ok.error == 0, "login accepts the issued token");
    Expect(login_ok.uid == 42, "login echoes uid");
    Expect(login_ok.token == first.token, "login echoes token");

    auto login_bad = core.Login(42, "bad-token");
    Expect(login_bad.error != 0, "login rejects an invalid token");

    std::cout << "StatusServerCoreTests passed" << std::endl;
    return 0;
}
