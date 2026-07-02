#include "socket_server.h"
#include <cstring>
#include <iostream>
#include <vector>

SocketServer::SocketServer(const std::string& path, Handler handler)
    : socket_path_(path), handler_(std::move(handler)), server_fd_(-1) {

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    unlink(path.c_str());

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 8) < 0) {
        perror("listen");
        close(server_fd_);
        server_fd_ = -1;
    }
}

SocketServer::~SocketServer() {
    if (server_fd_ >= 0) {
        close(server_fd_);
        unlink(socket_path_.c_str());
    }
}

void SocketServer::handle_client(int client_fd) {
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(client_fd); return; }
    buf[n] = '\0';

    std::string request(buf);
    std::string response = handler_(request);
    response += "\n";
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
}

void SocketServer::run(std::atomic<bool>& running) {
    if (server_fd_ < 0) return;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd_, &fds);

        struct timeval tv{1, 0};
        int ret = select(server_fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            std::thread(&SocketServer::handle_client, this, client).detach();
        }
    }
}
