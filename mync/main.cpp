
#include "stats.h"
#include "utils.h"
using namespace std;

#include <thread>
#include <liburing.h>
#include <stdio.h>
#include <fcntl.h>
#include "epoll.h"
#include "io_uring.h"

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

        std::thread ([=]() {
            char buf[BUFFER];
            ssize_t n;
            while ((n = recv(client_sock, buf, sizeof(buf), 0)) > 0) {
                // gcount_packets++;
                // gcount_bytes+=n;
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
        }).detach();

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
        // gcount_packets++;
        // gcount_bytes+=n;
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

    // gstartup = std::chrono::steady_clock::now();
    // signal(SIGINT, signal_handler);

    // if (is_listen){
    //     listen_mode(port);
    // }else{
    //     client_mode(host, port);
    // }

    if (is_listen){
        listen_mode_epoll(port);
    }else{
        client_mode_epoll(host, port);
    }

    // if (is_listen){
    //     listen_mode_iouring(port);
    // }else{
    //     client_mode_iouring(host, port);
    // }

    // new std::thread([&](){
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // });

    return 0;
}
