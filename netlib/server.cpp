#include "server.h"
#include <cassert>
// #include "serialization.h"

int IServer::create_listen_socket()
{
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        last_error_ = "socket failed";
        return -1;
    }

    // if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &conf_.recv_buffer_size, sizeof(conf_.recv_buffer_size)) == -1) {
    //     std::cout << "fail set SO_RCVBUF";
    // };

    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        std::cout << "fail set SO_RCVTIMEO 1s";
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        last_error_ = "setsockopt reuseaddr failed";
        close(sock);
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(conf_.port);
    if (inet_pton(AF_INET, conf_.host.c_str(), &address.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return -1;
    }

    if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        last_error_ = "bind failed";
        close(sock);
        return -1;
    }

    if (listen(sock, conf_.max_connections) < 0) {
        last_error_ = "listen failed";
        close(sock);
        return false;
    }

    return sock;
}

string IServer::GetServerState()
{
    switch (state_) {
    case ServerState::STOPPED: return "STOPPED";
    case ServerState::WAITING: return "WAITING";
    case ServerState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

SimpleServer::SimpleServer(ServerConfig config, EventDispatcher *e) :
    IServer(std::move(config)), dispatcher_(e){

    epoll_.SetOnReadAcceptHandler([&](int fd) {

        if (fd == listen_socket_) {
            // d("  handle accept recv ")
            handleAccept();
        } else {
            // d("  handle data ")
            // if (clients_[fd].state == ClientData::HANDSHAKE){
            //     handleHandshake(fd);
            // }else
            handleClientData(fd);
        }
    });

    epoll_.SetOnDisconnectHandler([&](int fd) {
        // d("(WARN) server epoll before disconnect")
        if (fd == listen_socket_) {
            Stop();
        } else if (fd > 0){
            removeClient(fd);
        }
        // d("(WARN) accept_epoll after disconnect")
    });
}

void SimpleServer::StartWait()
{
    // d("start wait epoll")
    state_ = ServerState::WAITING;
    epoll_.RunEpoll();
}

bool SimpleServer::StartListen(int num_workers){
    auto sock = create_listen_socket();
    if (sock < 0){
        state_ = ServerState::ERROR;
        return false;
    }

    state_ = ServerState::WAITING;

    listen_socket_ = sock;
    // accept сокет пока не должен иметь статистики, поэтому не добавляем сюда
    // AddClientFd(sock, {});
    epoll_.AddFd(sock);
    epoll_.RunEpoll();
    return true;
}

void SimpleServer::Stop(){
    state_ = ServerState::STOPPED;
    epoll_.RemoveFd(listen_socket_);
    epoll_.StopEpoll();
    listen_socket_ = -1;
}

int SimpleServer::CountClients(){
    return clients_.size();
}

void SimpleServer::AddClientFd(int fd, const Stats &st){
    // std::lock_guard<std::mutex> lock(clients_mtx_);
    // clients_.emplace(fd, ClientData{});
    // clients_[fd]->stats = std::move(st);

    auto client = std::make_shared<ClientData>();
    client->stats = std::move(st);
    {
        std::lock_guard<std::mutex> lock(clients_mtx_);
        clients_.emplace(fd, client);
    }


    epoll_.AddFd(fd);
    // preClient_socks_.push(std::make_pair(fd, std::move(st)));
}

void SimpleServer::AddHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void SimpleServer::handleAccept(){
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(listen_socket_, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        d(client_fd << " sim accept " << st.ip);

        // std::lock_guard<std::mutex> lock(clients_mtx_);
        // clients_.emplace(client_fd, ClientData{});
        // clients_[client_fd].stats = std::move(st);
        // epoll_.AddFd(client_fd);
        AddClientFd(client_fd, st);
        // clients_[client_fd].state = ClientData::HANDSHAKE;
    }
}

// bool SimpleServer::readIntoBuffer(int fd, ClientBuffer& buf)
// {
//     ssize_t n = recv(fd, temp, sizeof(temp), MSG_DONTWAIT);
    
//     if (n == 0) { removeClient(fd); return false; }
//     if (n < 0) {
//         if (errno != EAGAIN && errno != EWOULDBLOCK) removeClient(fd);
//         return false;
//     }
    
//     if (!buf.feed(temp, n)) {
//         std::cerr << "Invalid packet from fd " << fd << std::endl;
//         removeClient(fd);
//         return false;
//     }
//     return true;
// }


void SimpleServer::handleClientData(int fd)
{
    // ClientData& cli = clients_[fd];
    std::shared_ptr<ClientData> cli;
    {
        std::lock_guard<std::mutex> lock(clients_mtx_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) {
            return;
        }
        cli = it->second;
    }
    auto& pktParser = cli->pktReader_;
    auto& stats = cli->stats;

    while (true) {
        ssize_t szrecv = recv(fd, buffer_, sizeof(buffer_), MSG_DONTWAIT);
        if (szrecv <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            pktParser.Reset();
            removeClient(fd);
            return;
        }
        // stats_.addBytes(n);
        stats.addBytes(szrecv);
        // d("add bytes:" << n)

        int processed = 0;
        while (processed < szrecv) {
            int szreaded = pktParser.ParseDataPacket(buffer_ + processed, szrecv - processed);
            if (szreaded <= 0) break;
            // if (readed == 0) {
            //     if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            //     std::cerr << "Packet parsing error from fd " << fd << std::endl;
            //     pktParser.Reset();
            //     removeClient(fd);
            //     return;
            // }
            processed += szreaded;

            if (pktParser.IsPacketReady()) {
                // d("Received packet of size " << pktParser.GetPayloadSize() << " from fd " << fd);

                if (dispatcher_) {
                    DataReceived d;
                    d.data = const_cast<char*>(pktParser.GetPayloadData());
                    d.size = pktParser.GetPayloadSize();
                    dispatcher_->onEvent(EventType::DataReceived, &d);
                }

                pktParser.Reset();
            }
        }
    }
}

void SimpleServer::removeClient(int fd){
    d("srv remove client")
    epoll_.RemoveFd(fd);

    if (dispatcher_) {
        dispatcher_->onEvent(EventType::ClientDisconnected);
    }

    // std::lock_guard<std::mutex> lock(clients_mtx_);
    // clients_.erase(fd);
    std::shared_ptr<ClientData> cli;
    {
        std::lock_guard<std::mutex> lock(clients_mtx_);
        auto it = clients_.find(fd);
        if (it != clients_.end()) {
            cli = it->second;
            clients_.erase(it);
        }
    }
}

MultithreadServer::MultithreadServer(ServerConfig config) :
    IServer(std::move(config)){
    accept_epoll_.SetOnReadAcceptHandler([&](int fd) { handleAccept(fd); });
    // accept_epoll_.SetErrorHandler([&](int fd) {
    //     state_ = ServerState::ERROR;
    // });

    accept_epoll_.SetOnDisconnectHandler([&](int fd) {
        if (fd > 0){
            Stop();
        }
    });

    // accept_epoll_.SetOnReadyWriteHandler()
}

bool MultithreadServer::StartListen(int num_workers){
    if (num_workers == 0){
        return false;
    }
    auto sock = create_listen_socket();
    if (sock < 0){
        state_ = ServerState::ERROR;
        return false;
    }

    state_ = ServerState::WAITING;

    // worker_client_counts_.resize(num_workers, 0);
    // for (int i = 0; i < num_workers; ++i) {
    //     auto worker = std::make_unique<SimpleServer>(conf_, dispatcher_);
    //     worker->AddHandlerEvent(EventType::ClientDisconnected, [this, i](void*) {
    //         --worker_client_counts_[i];
    //     });
    //     worker->StartWait();
    //     workers_.push_back(std::move(worker));
    //     worker_client_counts_[i] = 0;
    //     // worker_client_counts_.push_back(0);
    // }
    worker_client_counts_.clear();
    // worker_client_counts_.reserve(num_workers);
    // worker_client_counts_.resize(num_workers);
    worker_client_counts_ = std::vector<std::atomic<int>>(num_workers);


    for (int i = 0; i < num_workers; ++i) {
        // worker_client_counts_.emplace_back(0);

        auto worker = std::make_unique<SimpleServer>(conf_, dispatcher_);
        worker->AddHandlerEvent(EventType::ClientDisconnected, [this, i](void*) {
            worker_client_counts_[i]--;
        });
        worker->StartWait();
        workers_.push_back(std::move(worker));
    }


    listen_socket_ = sock;
    accept_epoll_.AddFd(sock);
    accept_epoll_.RunEpoll();
    d("start srv epoll workers " << num_workers)

    return true;
}

void MultithreadServer::Stop(){
    state_ = ServerState::STOPPED;
    accept_epoll_.RemoveFd(listen_socket_);
    accept_epoll_.StopEpoll();

    //>?????? точно нужно
    for (auto &w : workers_) {
        w->Stop();
    }
    // обнулить worker_client_counts_ ???

    // for (auto &th : worker_threads_) {
    //     if (th.joinable()) th.join();
    // }
}

int MultithreadServer::CountClients(){
    // return std::accumulate(worker_client_counts_.begin(), worker_client_counts_.end(), 0);
    int total = 0;
    for (auto &c : worker_client_counts_) {
        total += c.load();
    }
    return total;
}

void MultithreadServer::AddHandlerEvent(EventType type, std::function<void (void *)> handler)
{
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void MultithreadServer::handleAccept(int fd){
    d("srv accept " << fd << " " << listen_socket_)
    // if (fd != listen_socket_){//??
    //     return;
    // }
    assert(fd == listen_socket_);
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(listen_socket_, (sockaddr*)&client_addr, &addr_len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return;
        }

        // Сохраняем для статистики
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        // Находим воркер с минимальным числом клиентов
        // auto it = std::min_element(worker_client_counts_.begin(), worker_client_counts_.end());
        auto it = std::min_element(
            worker_client_counts_.begin(),
            worker_client_counts_.end(),
            [](const std::atomic<int>& a, const std::atomic<int>& b) {
                return a.load() < b.load();
            }
            );
        int idx = std::distance(worker_client_counts_.begin(), it);

        d(client_fd << " accept " << st.ip << " min:" << idx)

        if (dispatcher_){
            dispatcher_->onEvent(EventType::ClientConnected);
        }

        // Добавлям сокет в epoll воркера
        workers_[idx]->AddClientFd(client_fd, st);
        worker_client_counts_[idx]++;
    }
}
