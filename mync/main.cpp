#include <cstring>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "stats.h"

using namespace std;

const int BUFFER = 1 * 1024 * 1024; // 1 MiB // 65KiB 65536
const int MAX_CONNECTIONS = 3;

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
#include <thread>
void listen_mode(int port)
{
    int sock = create_socket(true, "0.0.0.0", port);
    if (sock < 0){
        return;
    }

    while(1){
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int client_sock;
        if ((client_sock = accept(sock, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            if (client_sock == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::cout << "accept timeout" << std::endl;
                } else {
                    std::cerr << "accept error " << errno << std::endl;
                    close(sock);
                    return;
                }
            }
        }
        // close(sock);

        std::thread worker([client_sock]() {
            char buf[BUFFER];
            ssize_t n;
            while ((n = recv(client_sock, buf, sizeof(buf), 0)) > 0) {
                gcount_packets++;
                gcount_bytes+=n;
                if (write_to_stdout(buf, n) != 0) {
                    std::cerr << "write to stdout failed" << std::endl;
                    break;
                }
            }

            if (n == 0) {
                std::cerr << "client disconnected" << std::endl;
            } else if (n < 0) {
                std::cerr << "recv() failed: " << strerror(errno) << std::endl;
            }
            close(client_sock);
        });worker.detach();

    }

}

void client_mode(std::string host, int port)
{
    int sock = create_socket(false, host, port);
    if (sock < 0){
        return;
    }

    char buf[BUFFER];
    ssize_t n;
    while ((n = read_from_stdin(buf, sizeof(buf))) > 0) {
        gcount_packets++;
        gcount_bytes+=n;
        if (send(sock, buf, n, MSG_NOSIGNAL) != n) {
            std::cerr << "send() failed: " << strerror(errno) << std::endl;
            break;
        }
    }
    close(sock);
}

void print_usage(const char* program_name) {
    // std::cout << "Usage: " << program_name << "(-l) [host] [port] (host_from)\n"
    //           << "  -l  - listen mode\n"
    //           << "  host  - Tcp connect host (default: 127.0.0.1)\n"
    //           << "  port  - Tcp connect port number (default: 12345)\n"
    //           << "Examples:\n"
    //           << "  " << program_name << " \"127.0.0.1\" 12345\n";
    std::cout << "Usage: \n"
              << program_name << " -l 12345 (sock recv and send stdout)\n"
              << program_name << " \"127.0.0.1\" 12345 (read stdin and sock send)\n";
}

int main(int argc, char* argv[])
{
    if (argc == 1 || argc >= 2 && std::string(argv[1]) == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    int count = 1;
    bool is_listen = false;
    string host = "127.0.0.1";
    int port = -1;
    try {
        // if (string(argv[1]) == "-l") {
        //     is_listen = true;
        //     if (argc > 2) port = std::stoi(argv[2]);
        // } else {
        //     is_listen = false;
        //     if (argc > count) host = string(argv[count++]);
        //     if (argc > count) port = std::stoi(argv[count++]);
        // }

        if (argc == 3) {
            // "-l port" или "host port"
            if (string(argv[1]) == "-l") {
                is_listen = true;
                port = std::stoi(argv[2]);
            } else {
                host = argv[1];
                port = std::stoi(argv[2]);
            }
        } else if (argc == 2) {
            throw invalid_argument("Not enough arguments");
        } else {
            throw invalid_argument("Too many arguments");
        }

        // if (port < 0) throw std::invalid_argument("fail read port");
        if (port < 1 || port > 65535) {
            throw std::out_of_range("Port must be between 1 and 65535");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        return 1;
    }

    gstartup = std::chrono::steady_clock::now();
    signal(SIGINT, signal_handler);

    if (is_listen){
        listen_mode(port);
    }else{
        client_mode(host, port);
    }

    // new std::thread([&](){
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // });

    return 0;
}
