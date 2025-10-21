#include "server.h"
#include "serialization.h"

bool IServer::create_socket()
{
    state_ = ServerState::STARTING;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        state_ = ServerState::ERROR;
        return false;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &conf_.recv_buffer_size, sizeof(conf_.recv_buffer_size)) == -1) {
        std::cout << "fail set SO_RCVBUF";
    };

    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        std::cout << "fail set SO_RCVTIMEO 1s";
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

string IServer::getServerState()
{
    switch (state_) {
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

    MessageParser parser(socket_, conf_.recv_buffer_size);
    ParsedMessage msg{};
    int client_sock;
    sockaddr_in address;
    int addrlen = sizeof(address);

    while (true) {
        std::cout << "Waiting for a client..." << std::endl;
        if ((client_sock = accept(socket_, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            last_error_ = "accept failed";
            continue;
        }
        std::cout << "Client connected!" << std::endl;

        // connection handler - auth
        if (parser.readMessage(msg, false) && msg.type == MessageType::AUTH_REQUEST){
            //send AUTH_RESPONSE
            // fill msg need data
            auto uuid = msg.auth_response.client_uuid;
            parser.sendAuthResponce(msg, uuid, 0);// TODO:
        }else{
            close(client_sock);
            std::cout << "Not our client!" << std::endl;
            continue;
        }

        // data handle
        while (true) {
            if (parser.readMessage(msg) == false || msg.type != MessageType::DATA_PKT){
                continue;//?
            }
            stats_.addBytes(msg.size_header);

            // (записываем в очередь) или (отправляем куда надо)

            // write to file
            if (!conf_.filename.empty()){
                write2file(conf_.filename, buff.data(), count_read);
            }
        }


        // std::vector<char> buff(conf_.recv_buffer_size);
        // while (true) {
        //     ssize_t count_read = recv(client_sock, buff.data(), buff.size(), 0);
        //     if (count_read <= 0) {
        //         if (count_read == 0) {
        //             std::cout << "Client disconnected." << std::endl;
        //         } else {
        //             std::cout << "Receive error: " << strerror(errno) << std::endl;
        //         }
        //         break;
        //     }
        //     stats_.addBytes(count_read);
        //     // buff[count_read] = '\0';
        //     // std::cout << "Received: " << valread << " bytes\n";

        //     // write to file
        //     if (!conf_.filename.empty()){
        //         write2file(conf_.filename, buff.data(), count_read);
        //     }
        // }

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
