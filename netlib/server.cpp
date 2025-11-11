#include "server.h"
#include <cassert>
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
        d("  handle accept recv ")
        if (fd == listen_socket_) {
            handleAccept();
        } else {
            handleClientData(fd);
        }
    });

    epoll_.SetDisconnectHandler([&](int fd) {
        d("(WARN) server epoll before disconnect")
            if (fd == listen_socket_) {
            epoll_.StopEpoll();
        } else {
            removeClient(fd);
            if (dispatcher_) {
                dispatcher_->onEvent(EventType::ClientDisconnect);
            }
        }
        d("(WARN) accept_epoll after disconnect")
    });
}

void SimpleServer::StartWait()
{
    // d("start wait epoll")
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
    std::lock_guard lock(preClient_socks_mtx_);
    clients_[fd].stats = std::move(st);
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

        epoll_.AddFd(client_fd);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        std::lock_guard lock(preClient_socks_mtx_);
        clients_[client_fd].stats = std::move(st);
        clients_[client_fd].state = ClientData::HANDSHAKE;
    }
}

void SimpleServer::handleClientData(int fd)
{
    ClientBuffer& buf = clients_[fd].buf;

    d("-srv get " << ((clients_[fd].state == ClientData::HANDSHAKE)?"[handshake]":" [handle data]"))

    while (true) {
        // читаем 4 байта - size
        if (buf.header_read < sizeof(buf.header)) {
            ssize_t n = recv(fd, buf.header.bytes + buf.header_read,
                             sizeof(buf.header) - buf.header_read, MSG_DONTWAIT);
            if (n <= 0) {
                if (errno == EAGAIN) return;
                // handle_error(fd, n);
                return;
            }
            buf.header_read += n;

            if (buf.header_read < sizeof(buf.header)) continue; // Ждём ещё

            buf.payload_size = ntohl(buf.header.net_value); // Парсим размер

            // ВАЛИДАЦИЯ
            // if (buf.payload_size == 0) {
            //     std::cerr << "Invalid packet size: " << buf.payload_size << " from fd " << fd << std::endl;
            //     removeClient(fd);
            //     return;
            // }
            buf.payload.resize(buf.payload_size); // Выделяем/переиспользуем память
        }

        // читаем данные
        if (buf.payload_read < buf.payload_size) {
            ssize_t n = recv(fd, buf.payload.data() + buf.payload_read,
                             buf.payload_size - buf.payload_read, MSG_DONTWAIT);
            if (n <= 0) {
                if (errno == EAGAIN) return; // Данных больше нет — нормально
                // handle_error(fd, n);
                return;
            }
            buf.payload_read += n;

            if (buf.payload_read < buf.payload_size) continue; // Ждём ещё
        }

        // все прочитали
        d("(end read pkt) recv: " << buf.payload_size << " bytes from fd " << fd);

        // TODO(): можно ли тут без if?
        if (clients_[fd].state == ClientData::HANDSHAKE){
            ClientHiMsg* pmsg = reinterpret_cast<ClientHiMsg*>(buf.payload.data());
            clients_[fd].client_uuid = pmsg->uuid;

            // TODO(): error

            d("-srv end handshake. send answ. " << uuid_to_string(pmsg->uuid))

            // TODO нормальная десерелизация!!! - пока ок
            // TODO(): без size???
            // restore if needed and need send to socket answer
            ServerAnsHiMsg smsg;
            smsg.client_mode = ServerAnsHiMsg::SEND;
            smsg.client_uuid = pmsg->uuid;
            send(fd, &smsg, sizeof(smsg), MSG_DONTWAIT);
        }
        else if (dispatcher_) {
            DataReceived d;
            d.data = buf.payload.data();
            d.size = buf.payload.size();
            dispatcher_->onEvent(EventType::DataReceived, &d);
        }

        buf.reset();
    }


    // пытаемся прочитать size? чтобы узнать размер пакета
    // clients_[fd].stats.addBytes(size);

    // ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
    // if (n > 0) {


    //     d("pkt full recv: " << n << " bytes");
    //     if (dispatcher_) {
    //         dispatcher_->onEvent(EventType::DataReceived, &n);
    //     }

    // } else {
    //     removeClient(fd);
    //     if (dispatcher_) {
    //         dispatcher_->onEvent(EventType::ClientDisconnect);
    //     }
    // }
}

void SimpleServer::removeClient(int fd){
    d("srv remove client")
    epoll_.RemoveFd(fd);
    close(fd);

    std::lock_guard lock(preClient_socks_mtx_);
    clients_.erase(fd);
}

MultithreadServer::MultithreadServer(ServerConfig config) :
    IServer(std::move(config)){
    accept_epoll_.SetOnReadAcceptHandler([&](int fd) { handleAccept(fd); });
    // accept_epoll_.SetErrorHandler([&](int fd) {
    //     state_ = ServerState::ERROR;
    // });

    accept_epoll_.SetDisconnectHandler([&](int fd) {
        d("(WARN) server accept_epoll before disconnect")
        Stop();
        d("(WARN) server accept_epoll after disconnect")
    });
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

    worker_client_counts_.resize(num_workers, 0);
    for (int i = 0; i < num_workers; ++i) {
        auto worker = std::make_unique<SimpleServer>(conf_, dispatcher_);
        worker->AddHandlerEvent(EventType::ClientDisconnect, [this, i](void*) {
            --worker_client_counts_[i];
        });
        worker->StartWait();
        workers_.push_back(std::move(worker));
        worker_client_counts_[i] = 0;
        // worker_client_counts_.push_back(0);
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
}

int MultithreadServer::CountClients(){
    return std::accumulate(worker_client_counts_.begin(), worker_client_counts_.end(), 0);
}

void MultithreadServer::AddHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void MultithreadServer::handleAccept(int fd){
    // d("srv accept " << fd << " " << listen_socket_)
    // if (fd != listen_socket_){//??
    //     return;
    // }
    assert(fd == listen_socket_);
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

        d("accept " << st.ip << " min:" << idx)

        // Добавляет сокет в epoll воркера
        workers_[idx]->AddClientFd(client_fd, st);
        worker_client_counts_[idx]++;
    }
}
