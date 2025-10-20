#include "server.h"

bool IServer::create_socket()
{
    state_ = ServerState::STARTING;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        state_ = ServerState::ERROR;
        return false;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        last_error_ = "setsockopt reuseaddr failed";
        state_ = ServerState::ERROR;
        close(sock);
        return false;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(conf_.port);
    if (inet_pton(AF_INET, conf_.host.c_str(), &address.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        state_ = ServerState::ERROR;
        close(sock);
        return false;
    }

    if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        last_error_ = "bind failed";
        state_ = ServerState::ERROR;
        close(sock);
        return false;
    }

    if (listen(sock, conf_.max_connections) < 0) {
        last_error_ = "listen failed";
        state_ = ServerState::ERROR;
        close(sock);
        return false;
    }

    socket_ = sock;
    return true;
}

string IServer::getServerState(ServerState state)
{
    switch (state) {
    case ServerState::STARTING: return "STARTING";
    case ServerState::RUNNING: return "RUNNING";
    case ServerState::STOPPING: return "STOPPING";
    case ServerState::STOPPED: return "STOPPED";
    case ServerState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

bool SinglethreadServer::start()
{
    if (!create_socket()){
        return false;
    }

    state_ = ServerState::RUNNING;

    int client_sock;
    sockaddr_in address;
    int addrlen = sizeof(address);

    // print stats
    new std::thread([&](){
        while(socket_){
            std::cout << getServerState(state_) << " recv:" << stats_.getBitrate() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    while (true) {
        std::cout << "Waiting for a client..." << std::endl;
        if ((client_sock = accept(socket_, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            last_error_ = "accept failed";
            continue;
        }

        std::cout << "Client connected!" << std::endl;


        char *buffer = new char[conf_.recv_buffer_size];
        while (true) {
            ssize_t count_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (count_read <= 0) {
                if (count_read == 0) {
                    std::cout << "Client disconnected." << std::endl;
                } else {
                    std::cout << "Receive error: " << strerror(errno) << std::endl;
                }
                break;
            }
            stats_.addBytes(count_read);
            buffer[count_read] = '\0';
            // std::cout << "Received: " << valread << " bytes\n";

            // answer
            // std::string response = "Echo: " + std::string(buffer);
            // send(client_sock, response.c_str(), response.length(), 0);

            // write to file
            if (!conf_.filename.empty()){
                write2file(conf_.filename, buffer, count_read);
            }
        }

        close(client_sock);
        std::cout << "Closed connection. Waiting for new client..." << std::endl;;
    }

    close(socket_);
    state_ = ServerState::STOPPED;
}

bool MultithreadServer::start()
{
    //accept in n threads
}
