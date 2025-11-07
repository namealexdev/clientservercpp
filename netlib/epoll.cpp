#include "epoll.h"

bool BaseEpoll::add_fd(int fd, uint32_t events)
{
    epoll_event ev{.events = events, .data{.fd = fd}};
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // throw std::runtime_error("epoll_ctl add");
        std::cout << "fail epoll_ctl add " << fd << " error: " << strerror(errno)<< std::endl;
        return false;
    }
    return true;
}

void BaseEpoll::remove_fd(int fd)
{
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
}

void BaseEpoll::start(){
    if (thLoop_){
        stop();
    }
    // if (!thLoop_){
    thLoop_ = new std::thread([this](){execLoop();});
}

void BaseEpoll::stop(){
    if (thLoop_){
        need_stop_ = true;
        thLoop_->join();
        delete thLoop_;
        thLoop_ = nullptr;
    }
}

BaseEpoll::BaseEpoll(){
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ == -1) throw std::runtime_error("epoll_create1");
}

BaseEpoll::~BaseEpoll(){
    if (epfd_ >= 0) {
        close(epfd_);
    }
}

void BaseEpoll::execLoop()
{
    static epoll_event events[MAX_EVENTS];

    while (!need_stop_) {
        int nfds = epoll_wait(epfd_, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("epoll_wait");
        }

        for (int i = 0; i < nfds; ++i) {
            if (onEvent){
                onEvent(events[i].data.fd, events[i].events);
            }
        }
    }
}
// Явно инстанцируем шаблон для нужного типа
// template void BaseEpoll<LightEpoll>::exec();
/*
ClientLightEpoll::ClientLightEpoll(IClientEventHandler* clh) {
    clientHandler_ = clh;
    onEvent = [this](int fd, uint32_t evs) {
        on_epoll_event(fd, evs);
    };
}

void ClientLightEpoll::start_handle(int sock){
    if (socket_ > 0)
        throw std::runtime_error("cli wrong use start_handle ");
    if (!add_fd(sock, EPOLLIN | EPOLLRDHUP)){
        return;
    }
    socket_ = sock;
    handleth_ = new std::thread([=](){
        exec();
    });
}

void ClientLightEpoll::stop(){
    need_stop_ = true;
    if (handleth_){
        handleth_->join();
        delete handleth_;
        handleth_ = nullptr;
    }
    close(socket_);
    socket_ = -1;
}

void ClientLightEpoll::send(char *d, int sz){
    if (::send(socket_, d, sz, MSG_NOSIGNAL) != sz) {
        std::cerr << socket_ << " send() failed: " << strerror(errno) << std::endl;
    }
}

void ClientLightEpoll::queue_add(char *d, int sz){
    queue_.push(std::make_pair(d, sz));
}

void ClientLightEpoll::queue_send(){
    while(!queue_.empty()){
        auto el = queue_.front();
        queue_.pop();
        send(el.first, el.second);
    }
}

void ClientLightEpoll::on_epoll_event(int fd, uint32_t evs){
    if (need_stop_){
        return;
    }
    // close socket
    if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        close(socket_);
        socket_ = -1;
        d("close client " << fd);
        clientHandler_->onEvent(need_reconnect_ ? EventType::Reconnected : EventType::Disconnected);
        if (need_reconnect_){
            // reconnect();
        }
        // close(socket_);
        // socket_ = 0;
        //reconnect?
        return;
    }

    // тут один единственный сокет, поэтому без проверок
    handle_socket_data();

    if (socket_ == -1 && need_reconnect_){
        // reconnect();
    }
}

void ClientLightEpoll::handle_socket_data(){
    ssize_t n;
    // это не SubEpoll, тут не нужна статистика
    // std::cout << "2handle_socket_data " << n << std::endl;
    n = recv(socket_, buffer, sizeof(buffer), 0);
    if (n > 0) {
        // if (on_recv_handler)
        //     on_recv_handler(buffer, n);
        // if (write_to_stdout(buffer, n) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
    } else if (n == 0) {
        close(socket_);
        socket_ = -1;
    } else {
        throw std::runtime_error("recv from socket");
    }
}

ServerLightEpoll::ServerLightEpoll(IClientEventHandler* clh){
    clientHandler_ = clh;
    onEvent = [this](int fd, uint32_t evs) {
        on_epoll_event(fd, evs);
    };
}

int ServerLightEpoll::countClients(){
    return clients.size();
}

void ServerLightEpoll::start_handle(int sock){
    if (socket_ > 0)
        throw std::runtime_error("srv wrong use start_handle ");
    if (sock > 0 && !add_fd(sock, EPOLLIN | EPOLLRDHUP)){
        return;
    }
    socket_ = sock;
    handleth_ = new std::thread([&](){
        exec();
    });
}

void ServerLightEpoll::stop(){
    need_stop_ = true;
    if (handleth_){
        handleth_->join();
        delete handleth_;
        handleth_ = nullptr;
    }

    if (socket_ > 0)
        close(socket_);

    d("stop server:" << clients.size())
    while(!clients.empty()){
        remove_client(clients.begin()->first);
    }
}

void ServerLightEpoll::on_epoll_event(int fd, uint32_t evs){
    if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        if (fd == socket_) {
            need_stop_ = true;
            d("need stop server epoll " << socket_)
        } else {
            remove_client(fd);
            std::cout << "server remove client " << fd << std::endl;
            clientHandler_->onEvent(EventType::ClientDisconnect);
        }
        return;
    }

    if (evs & EPOLLIN) {
        if (socket_ > 0 && fd == socket_) handle_accept();
        else if (clients.count(fd)) handle_client_data(fd);
    }
}

void ServerLightEpoll::remove_client(int fd) {
    d("remove_client " << fd)
    // count_fd--;
    remove_fd(fd);
    {
        // std::unique_lock lock(mtx_clients);// запись
        clients.erase(fd);
    }

    // size_clients--;
    close(fd);
}

void ServerLightEpoll::handle_client_data(int fd) {
    ssize_t n;
    n = recv(fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        // std::cout << "1handle_client_data " << n << std::endl;

        {
            // std::unique_lock lock(mtx_clients); // чтение - Запись? unique_lock
            clients[fd].addBytes(n);
        }
        // if (write_to_stdout(buffer, SERVER_WRITE_STDOUT?n:0) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
    } else {
        remove_client(fd);
    }
}

void ServerLightEpoll::handle_accept()
{
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(socket_, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            throw std::runtime_error(std::string("accept4: ") + strerror(errno));
        }

        // Увеличение буфера отправки
        // const int bufsize = BUF_SIZE;
        // if (setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        //     perror("setsockopt SO_SNDBUF");
        // }

        // // Увеличение буфера приема
        // if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        //     perror("setsockopt SO_RCVBUF");
        // }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        // тут надо распределять это все по потокам
        // if (!balance_socket(client_fd, st)){
        //     // если надо добавить все в текущем потоке, то очередь для сокетов не нужна
        //     // std::cout << "not balance_socket " << std::endl;
        //     if (!add_fd(client_fd, EPOLLIN | EPOLLRDHUP )){
        //         close(client_fd);
        //     }else{
        //         size_clients++;
        clients.emplace(client_fd, std::move(st));
        d("add_client " << client_fd);
            // std::cout << "Added socket: " << client_fd << " (" << clients[client_fd].ip << ")" << std::endl;
        //     }

        // }

    }
}

ClientMultithEpoll::ClientMultithEpoll(IClientEventHandler *clh){
    clientHandler_ = clh;
    onEvent = [this](int fd, uint32_t evs) {
        on_epoll_event(fd, evs);
    };
}

void ClientMultithEpoll::start_handle(int sock){
    if (socket_ > 0)
        throw std::runtime_error("cli wrong use start_handle ");
    if (!add_fd(sock, EPOLLIN | EPOLLRDHUP)){
        return;
    }
    socket_ = sock;
    handleth_ = new std::thread([=](){
        exec();
    });
}

void ClientMultithEpoll::stop(){
    need_stop_ = true;
    if (handleth_){
        handleth_->join();
        delete handleth_;
        handleth_ = nullptr;
    }
    close(socket_);
    socket_ = -1;
}

void ClientMultithEpoll::send(char *d, int sz){
    if (::send(socket_, d, sz, MSG_NOSIGNAL) != sz) {
        std::cerr << socket_ << " send() failed: " << strerror(errno) << std::endl;
    }
}

void ClientMultithEpoll::queue_add(char *d, int sz){
    std::lock_guard<std::mutex> lock(mtx_queue_);
    queue_.push(std::make_pair(d, sz));
}

void ClientMultithEpoll::queue_send(){
    std::lock_guard<std::mutex> lock(mtx_queue_);
    while(!queue_.empty()){
        auto el = queue_.front();
        queue_.pop();
        send(el.first, el.second);
    }
}

void ClientMultithEpoll::on_epoll_event(int fd, uint32_t evs){
    if (need_stop_){
        return;
    }
    // close socket
    if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        close(socket_);
        socket_ = -1;
        d("close client " << fd);
        clientHandler_->onEvent(need_reconnect_ ? EventType::Reconnected : EventType::Disconnected);
        if (need_reconnect_){
            // reconnect();
        }
        // close(socket_);
        // socket_ = 0;
        //reconnect?
        return;
    }

    // тут один единственный сокет, поэтому без проверок
    handle_socket_data();

    if (socket_ == -1 && need_reconnect_){
        // reconnect();
    }
}

void ClientMultithEpoll::handle_socket_data(){
    ssize_t n;
    // это не SubEpoll, тут не нужна статистика
    // std::cout << "2handle_socket_data " << n << std::endl;
    n = recv(socket_, buffer, sizeof(buffer), 0);
    if (n > 0) {
        // clientHandler_->onEvent()
        // if (on_recv_handler)
        //     on_recv_handler(buffer, n);
        // if (write_to_stdout(buffer, n) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
    } else if (n == 0) {
        close(socket_);
        socket_ = -1;
    } else {
        throw std::runtime_error("recv from socket");
    }
}

void ClientMultithEpoll::start_queue(){
    queue_th_ = new std::thread([&](){
        while (!need_stop_){
            if (queue_.empty()){
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            queue_send();
        }
    });
}

ServerMultithEpoll::ServerMultithEpoll(IClientEventHandler *clh){
    clientHandler_ = clh;
    onEvent = [this](int fd, uint32_t evs) {
        on_epoll_event(fd, evs);
    };
}

ServerMultithEpoll::~ServerMultithEpoll() {
    for (auto& e : subepolls_) {
        e->stop();
    }
    subepolls_.clear();
}

void ServerMultithEpoll::start_handle(int sock, int count_workers){
    if (socket_ > 0)
        throw std::runtime_error("srv wrong use start_handle ");
    if (sock > 0 && !add_fd(sock, EPOLLIN | EPOLLRDHUP)){
        return;
    }
    socket_ = sock;
    handleth_ = new std::thread([&](){
        exec();
    });

    for (size_t i = 0; i < count_workers; ++i) {
        auto* subepoll = new ServerSubEpoll();
        subepoll->start_handle(-1);
    }
}

void ServerMultithEpoll::stop(){
    need_stop_ = true;
    if (handleth_){
        handleth_->join();
        delete handleth_;
        handleth_ = nullptr;
    }

    if (socket_ > 0)
        close(socket_);

}

int ServerMultithEpoll::countClients(){
    int full = 0;
    for (auto e: subepolls_){
        full += e->countClients();
    }
    return full;
}

void ServerMultithEpoll::on_epoll_event(int fd, uint32_t evs){
    if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        if (fd == socket_) {
            need_stop_ = true;
            d("need stop server epoll " << socket_)
        }
        return;
    }

    if (evs & EPOLLIN) {
        // if (socket_ > 0 && fd == socket_)
        handle_accept();
    }
}

void ServerMultithEpoll::handle_accept(){
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(socket_, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            throw std::runtime_error(std::string("accept4: ") + strerror(errno));
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        //balance_socket
        auto* min_subepoll = *std::min_element (subepolls_.begin(), subepolls_.end(), [](auto* a, auto* b) {
            return a->countClients() < b->countClients();
        });
        min_subepoll->push_external_socket(client_fd, st);
    }


}

ServerSubEpoll::ServerSubEpoll(){
    onEvent = [this](int fd, uint32_t evs) {
        on_epoll_event(fd, evs);
    };
}

void ServerSubEpoll::start_handle(int sock){
    if (socket_ > 0)
        throw std::runtime_error("srv wrong use start_handle ");
    if (sock > 0 && !add_fd(sock, EPOLLIN | EPOLLRDHUP)){
        return;
    }
    socket_ = sock;
    handleth_ = new std::thread([&](){
        exec();
    });
}

void ServerSubEpoll::stop(){
    need_stop_ = true;
    if (handleth_){
        handleth_->join();
        delete handleth_;
        handleth_ = nullptr;
    }

    if (socket_ > 0)
        close(socket_);

    d("stop server:" << clients.size())
        while(!clients.empty()){
        remove_client(clients.begin()->first);
    }
}

int ServerSubEpoll::countClients(){
    return clients.size();
}

void ServerSubEpoll::push_external_socket(int client_fd, const Stats &st){
    {
        std::lock_guard lock(mtx_pending_new_socks_);
        pending_new_socks_.push(std::make_pair(client_fd, std::move(st)));
        // добавляем тут, чтобы балансировка проходила корректно
        // size_clients++;
    }
    // uint64_t one = 1;
    // write(wakeup_fd, &one, sizeof(one)); // разбудить epoll
}

void ServerSubEpoll::on_epoll_event(int fd, uint32_t evs){
    if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        if (fd == socket_) {
            need_stop_ = true;
            d("need stop server epoll " << socket_)
        } else {
            remove_client(fd);
            std::cout << "server remove client " << fd << std::endl;
            // clientHandler_->onEvent(EventType::ClientDisconnect);
        }
        return;
    }

    if (evs & EPOLLIN) {
        // if (socket_ > 0 && fd == socket_) handle_accept();
        // else if (clients.count(fd)) handle_client_data(fd);

        // тут только клиенты, так что без проверки if (clients.count(fd))
        handle_client_data(fd);
    }
}

void ServerSubEpoll::remove_client(int fd){
    d("remove_client " << fd)
        // count_fd--;
        remove_fd(fd);
    {
        // std::unique_lock lock(mtx_clients);// запись
        clients.erase(fd);
    }

    // size_clients--;
    close(fd);
}

void ServerSubEpoll::handle_client_data(int fd){
    ssize_t n;
    n = recv(fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        // std::cout << "1handle_client_data " << n << std::endl;

        {
            // std::unique_lock lock(mtx_clients); // чтение - Запись? unique_lock
            clients[fd].addBytes(n);
        }
        // if (write_to_stdout(buffer, SERVER_WRITE_STDOUT?n:0) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
    } else {
        remove_client(fd);
    }
}
*/
