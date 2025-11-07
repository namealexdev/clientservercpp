#include "client.h"
// #include "serialization.h"

int IClient::create_socket_connect()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        return -1;
    }

    // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &conf_.send_buffer_size, sizeof(conf_.send_buffer_size));

    if (!conf_.host.empty()){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, conf_.host.c_str(), &bind_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton bind failed " + conf_.host;
            close(sock);
            return -1;
        }
        if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            last_error_ = "bind failed";
            close(sock);
            return -1;
        }
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf_.server_port);
    if (inet_pton(AF_INET, conf_.server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return -1;
    }

    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        close(sock);
        return -1;
    }

    return sock;
}

string IClient::getClientState()
{
    switch (state_) {
    case ClientState::DISCONNECTED: return "DISCONNECTED";
    case ClientState::HANDSHAKE: return "HANDSHAKE";
    case ClientState::WAITING: return "WAITING";
    case ClientState::SENDING: return "SENDING";
    case ClientState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

SimpleClient::SimpleClient(ClientConfig config):
    IClient(std::move(config)) {
    epoll_.onEvent = [this](int fd, uint32_t events) { onEpollEvent(fd, events); };
}

SimpleClient::~SimpleClient()
{
    epoll_.stop();
    if(queue_th_){
        queue_th_->join();
        delete queue_th_;
    }
}

void SimpleClient::connect(){
    auto sock = create_socket_connect();
    if (sock < 0){
        state_ = ClientState::ERROR;
        return ;
    }

    // TODO: add handshake
    // state_ = ClientState::HANDSHAKE;
    // TODO: если в очереди есть данные отправляем

    state_ = ClientState::WAITING;
    epoll_.add_fd(sock, EPOLLIN | EPOLLRDHUP);
    epoll_.start();
}

void SimpleClient::disconnect(){
    state_ = ClientState::DISCONNECTED;
    epoll_.need_stop_ = true;
    // epoll_.stop();
}

void SimpleClient::send(char *data, int size){
    if (::send(socket_, data, size, MSG_NOSIGNAL) != size) {
        std::cerr << socket_ << " send() failed: " << strerror(errno) << std::endl;
    }
}

void SimpleClient::queueAdd(char *data, int size){
    std::lock_guard<std::mutex> lock(queue_mtx_);
    queue_.push(std::make_pair(data, size));
}

void SimpleClient::queueSend(){
    std::lock_guard<std::mutex> lock(queue_mtx_);
    while(!queue_.empty()){
        auto el = queue_.front();
        queue_.pop();
        send(el.first, el.second);
    }
}

void SimpleClient::start_async_queue()
{
    queue_th_ = new std::thread([&](){
        while (socket_ > 0){
            if (queue_.empty()){
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            queueSend();
        }
    });
}

void SimpleClient::addHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

void SimpleClient::onEpollEvent(int fd, uint32_t events){
    // Обрабатывает события от epoll: отключение, чтение и т.д.
    if (events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        if (dispatcher_)
            dispatcher_->onEvent(EventType::Disconnected);
        state_ = ClientState::DISCONNECTED;
        close(socket_);
        socket_ = -1;
        return;
    }

    if (events & EPOLLIN) {
        handleData();
    }
}

void SimpleClient::handleData(){
    ssize_t n;
    // это не SubEpoll, тут не нужна статистика
    // std::cout << "2handle_socket_data " << n << std::endl;
    n = recv(socket_, buffer, sizeof(buffer), 0);
    if (n > 0) {
        if (dispatcher_)
            dispatcher_->onEvent(EventType::DataReceived, &n);
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

