#include "utils.h"
#include <random>
// return count bytes, 0 - stdin закрыт, -1 error
ssize_t read_from_stdin(char *buf, size_t maxlen) {
    ssize_t n;
    do {
        n = read(STDIN_FILENO, buf, maxlen);
        // std::cout <<"in read " << n << std::endl;
    } while (n == -1 && errno == EINTR); // повтор при прерывании сигналом
    // std::cout <<"stop " << n << std::endl;
    return n;
}

// // 0 ok, -1 error
// int write_to_stdout(const void *buf, size_t len) {
//     const char *p = (const char *)buf;
//     size_t written = 0;
//     ssize_t n;

//     while (written < len) {
//         n = write(STDOUT_FILENO, p + written, len - written);
//         if (n == -1) {
//             if (errno == EINTR) {
//                 continue; // повторить
//             }
//             return -1;
//         }
//         written += n;
//     }
//     return 0;
// }

// ssize_t read_from_stdin(char* buffer, size_t size) {
//     return read(STDIN_FILENO, buffer, size);
// }

int write_to_stdout(const char* buffer, size_t size) {
    // return 0;
    return write(STDOUT_FILENO, buffer, size) == (ssize_t)size ? 0 : -1;
}

int create_socket(bool islisten, const std::string& host, int port)
{
    int sock = -1;
    int type = SOCK_STREAM;
    if (islisten) type |= SOCK_NONBLOCK;
    if ((sock = socket(AF_INET, type, 0)) == 0) {
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

// #ifdef TCP_NODELAY
//     // для тестов
//     std::cout << "TCP_NODELAY available" << std::endl;
//     int nodelay = 1;
//     setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
// #else
//     std::cout << "TCP_NODELAY NOT available" << std::endl;
// #endif

    if (islisten){
        if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "listen bind failed" << std::endl;
            close(sock);
            return -1;
        }

        // int flags = fcntl(sock, F_GETFL, 0);
        // if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        //     // perror("fcntl O_NONBLOCK");
        //     std::cerr << "fcntl O_NONBLOCK failed" << std::endl;
        //     close(sock);
        //     return -1;
        // }

        if (listen(sock, MAX_CONNECTIONS) < 0) {
            std::cerr << "listen failed" << std::endl;
            close(sock);
            return -1;
        }
    }else{
        if (::connect(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "connect failed" << std::endl;
            close(sock);
            return -1;
        }
    }

    return sock;
}
