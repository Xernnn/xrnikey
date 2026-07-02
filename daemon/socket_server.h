#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class SocketServer {
public:
    using Handler = std::function<std::string(const std::string&)>;

    SocketServer(const std::string& path, Handler handler);
    ~SocketServer();

    void run(std::atomic<bool>& running);

private:
    std::string socket_path_;
    Handler handler_;
    int server_fd_;

    void handle_client(int client_fd);
};
