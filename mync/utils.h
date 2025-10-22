#ifndef UTILS_H
#define UTILS_H

const int BUFFER = 1 * 1024 * 1024; // 1 MiB // 65KiB 65536
const int MAX_CONNECTIONS = 3;

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// return count bytes, 0 - stdin закрыт, -1 error
ssize_t read_from_stdin(void *buf, size_t maxlen) {
    ssize_t n;
    do {
        n = read(STDIN_FILENO, buf, maxlen);
    } while (n == -1 && errno == EINTR); // повтор при прерывании сигналом

    return n;
}

// 0 ok, -1 error
int write_to_stdout(const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t written = 0;
    ssize_t n;

    while (written < len) {
        n = write(STDOUT_FILENO, p + written, len - written);
        if (n == -1) {
            if (errno == EINTR) {
                continue; // прерван сигналом — повторить
            }
            return -1;
        }
        written += n;
    }
    return 0;
}

int create_socket(bool islisten, const std::string& host, int port)
{
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "fail create socket" << std::endl;
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &BUFFER, sizeof(BUFFER)) == -1) {
        std::cout << "fail set SO_RCVBUF" << std::endl;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &BUFFER, sizeof(BUFFER)) == -1) {
        std::cout << "fail set SO_SNDBUF" << std::endl;
    }
    // timeval tv;
    // tv.tv_sec = 1;
    // tv.tv_usec = 0;
    // if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
    //     std::cout << "fail set SO_RCVTIMEO" << std::endl;
    // }

    int opt = islisten;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "setsockopt reuseaddr failed" << std::endl;
        close(sock);
        return -1;
    }

    // struct sockaddr_in address;
    // address.sin_family = AF_INET;
    // address.sin_port = htons(port);
    // if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
    //     std::cerr << "set address failed to " << host << std::endl;
    //     close(sock);
    //     return -1;
    // }

    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // или AF_UNSPEC для IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "getaddrinfo failed for host: " << host << std::endl;
        close(sock);
        return -1;
    }
    // Копируем первый результат в struct sockaddr_in
    struct sockaddr_in address = {};
    address = *(struct sockaddr_in*)res->ai_addr;
    address.sin_port = htons(port);
    freeaddrinfo(res);

    if (islisten){
        if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "bind failed" << std::endl;
            close(sock);
            return -1;
        }

        if (listen(sock, MAX_CONNECTIONS) < 0) {
            std::cerr << "listen failed" << std::endl;
            close(sock);
            return -1;
        }
    }else{
        if (::connect(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "connection failed" << std::endl;
            close(sock);
            return -1;
        }
    }

    return sock;
}

#endif // UTILS_H
