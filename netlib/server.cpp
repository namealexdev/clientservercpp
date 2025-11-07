#include "server.h"
// #include "serialization.h"

int IServer::create_listen_socket()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
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
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
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

string IServer::getServerState()
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
    epoll_.onEvent = [this](int fd, uint32_t events) { onEpollEvent(fd, events); };
}

bool SimpleServer::start(int num_workers){
    auto sock = create_listen_socket();
    if (sock < 0){
        state_ = ServerState::ERROR;
        return false;
    }

    state_ = ServerState::WAITING;

    epoll_.add_fd(sock, EPOLLIN | EPOLLRDHUP);
    epoll_.start();
    return true;
}

void SimpleServer::stop(){
    epoll_.need_stop_ = true;
    // epoll_.stop();
    if (listen_socket_ > 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
}

int SimpleServer::countClients(){
    return clients_.size();
}

bool SimpleServer::addClientFd(int fd, const Stats &st){
    std::lock_guard lock(preClient_socks_mtx_);
    preClient_socks_.push(std::make_pair(fd, std::move(st)));
}

void SimpleServer::addHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void SimpleServer::onEpollEvent(int fd, uint32_t events){
    // Обрабатывает события: accept или данные от клиента
    if (events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        if (fd == listen_socket_) {
            epoll_.need_stop_ = true;
        } else {
            removeClient(fd);
            if (dispatcher_)
                dispatcher_->onEvent(EventType::ClientDisconnect);
        }
        return;
    }

    if (events & EPOLLIN) {
        if (fd == listen_socket_) {
            handleAccept();
        } else {
            handleClientData(fd);
        }
    }
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

        epoll_.add_fd(client_fd, EPOLLIN | EPOLLRDHUP);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        clients_[client_fd] = std::move(st);

        if (dispatcher_)
            dispatcher_->onEvent(EventType::ClientConnect);
    }
}

void SimpleServer::handleClientData(int fd){
    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        clients_[fd].addBytes(n);
        if (dispatcher_)
            dispatcher_->onEvent(EventType::DataReceived, &n);
    } else {
        removeClient(fd);
        if (dispatcher_)
            dispatcher_->onEvent(EventType::ClientDisconnect);
    }
}

void SimpleServer::removeClient(int fd){
    epoll_.remove_fd(fd);
    clients_.erase(fd);
    close(fd);
}

MultithreadServer::MultithreadServer(ServerConfig config) :
    IServer(std::move(config)){
    accept_epoll_.onEvent = [this](int fd, uint32_t events) { onEpollEvent(fd, events); };
}

bool MultithreadServer::start(int num_workers){
    auto sock = create_listen_socket();
    if (sock < 0){
        state_ = ServerState::ERROR;
        return false;
    }

    state_ = ServerState::WAITING;

    worker_client_counts_.reserve(num_workers);
    for (int i = 0; i < num_workers; ++i) {
        auto worker = std::make_unique<SimpleServer>(conf_, dispatcher_);
        worker->addHandlerEvent(EventType::ClientDisconnect, [this, i](void*) {
            --worker_client_counts_[i];
        });
        workers_.push_back(std::move(worker));
        worker_client_counts_[i] = 0;
        // worker_client_counts_.push_back(0);
    }

    accept_epoll_.add_fd(sock, EPOLLIN | EPOLLRDHUP);
    accept_epoll_.start();

    return true;
}

void MultithreadServer::stop(){
    state_ = ServerState::STOPPED;
    accept_epoll_.need_stop_ = true;
    // accept_epoll_.stop();
}

int MultithreadServer::countClients(){
    return std::accumulate(worker_client_counts_.begin(), worker_client_counts_.end(), 0);
}

void MultithreadServer::addHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void MultithreadServer::onEpollEvent(int fd, uint32_t events){
    // Обрабатывает события только от listen сокета (accept)
    if (events & EPOLLIN && fd == listen_socket_) {
        handleAccept();
    }
}

void MultithreadServer::handleAccept(){
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

        // Находит воркер с минимальным числом клиентов
        auto it = std::min_element(worker_client_counts_.begin(), worker_client_counts_.end());
        int idx = std::distance(worker_client_counts_.begin(), it);

        // Добавляет сокет в epoll воркера
        workers_[idx]->addClientFd(client_fd, st);
        worker_client_counts_[idx]++;
    }
}
