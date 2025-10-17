#ifndef SERVER_H
#define SERVER_H

#include "const.h"

struct ServerConfig{
    string host;
    int port;

    int recv_buffer_size = 1024;
    int max_connections = 10;
};

enum class ServerState : uint8_t{
    UNINITIALIZED,
    INITIALIZED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR
};

class IServer{
public:
    virtual ~IServer() = default;
    virtual bool start() = 0; // wait accept

    ServerConfig conf_;
    string last_error_;
    int socket_ = 0;
    ServerState state_;
};

#include <fstream>
#include <filesystem>


#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

void write2file(const char* data, ssize_t size) {
    static const char* filename = "output.txt";
    static const off_t MAX_SIZE = 1ULL * 1024 * 1024 * 1024; // 1 GiB
    static int fd = -1;

    if (fd == -1) {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0644);
        if (fd == -1) return;
    }

    // check size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd); fd = -1;
        return;
    }
    if (st.st_size >= MAX_SIZE) {
        ftruncate(fd, 0); // reset
    }

    //full write
    int write_size = 0;
    while(write_size != size){
        int wr = write(fd, data+write_size, size-write_size);
        if (wr <= 0) {
            perror("write to file failed");
            break;
        }
        write_size += wr;
    }

    fsync(fd);//flush
}

class SinglethreadServer : public IServer {
public:
    SinglethreadServer(ServerConfig &conf){
        conf_ = std::move(conf);
    }
    bool start(){
        if (!create_socket()){
            return false;
        }

        int client_sock;
        sockaddr_in address;
        int addrlen = sizeof(address);
        while (true) {
            std::cout << "Waiting for a client..." << std::endl;;
            if ((client_sock = accept(socket_, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept failed");
                last_error_ = "accept failed";
                continue;
            }

            std::cout << "Client connected!" << std::endl;;

            char buffer[1024] = {0};
            while (true) {
                ssize_t valread = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (valread <= 0) {
                    if (valread == 0) {
                        std::cout << "Client disconnected." << std::endl;;
                    } else {
                        std::cout << "Receive error: " << strerror(errno) << std::endl;
                    }
                    break;
                }

                buffer[valread] = '\0';
                std::cout << "Received: " << valread << " bytes\n";

                // answer
                // std::string response = "Echo: " + std::string(buffer);
                // send(client_sock, response.c_str(), response.length(), 0);

                // write to file
                write2file(buffer, valread);
            }

            close(client_sock);
            std::cout << "Closed connection. Waiting for new client..." << std::endl;;
        }

        close(socket_);
    }

    bool create_socket(){
        int sock;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            last_error_ = "socket failed";
            return false;
        }

        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            last_error_ = "setsockopt reuseaddr failed";
            close(sock);
            return false;
        }

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(conf_.port);
        if (inet_pton(AF_INET, conf_.host.c_str(), &address.sin_addr) <= 0) {
            last_error_ = "inet_pton failed " + conf_.host;
            close(sock);
            return false;
        }

        if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            last_error_ = "bind failed";
            close(sock);
            return false;
        }

        if (listen(sock, conf_.max_connections) < 0) {
            last_error_ = "listen failed";
            close(sock);
            return false;
        }

        socket_ = sock;
        return true;
    }
};

class MultithreadServer : public IServer {
public:
    MultithreadServer(ServerConfig &conf){
        conf_ = std::move(conf);
    }
    bool start(){
        //accept in n threads
    }
};

// string serverStateToString(ServerState state) {
//     switch (state) {
//     case ServerState::UNINITIALIZED: return "UNINITIALIZED";
//     case ServerState::INITIALIZED: return "INITIALIZED";
//     case ServerState::STARTING: return "STARTING";
//     case ServerState::RUNNING: return "RUNNING";
//     case ServerState::STOPPING: return "STOPPING";
//     case ServerState::STOPPED: return "STOPPED";
//     case ServerState::ERROR: return "ERROR";
//     default: return "UNKNOWN";
//     }
// }

#endif // SERVER_H
