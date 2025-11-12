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
        if (bind(sock, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
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

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        close(sock);
        return -1;
    }

    return sock;
}

string IClient::GetClientState()
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

    epoll_.SetOnReadAcceptHandler([&](int fd) {
        // accept тут нету
        handleData();
    });

    epoll_.SetDisconnectHandler([&](int fd) {
        // d("(WARN) client epoll before disconnect")
        if (dispatcher_) {
            dispatcher_->onEvent(EventType::ClientDisconnected);
        }
        Disconnect();
        // d("(WARN) client epoll after disconnect")
    });
}

SimpleClient::~SimpleClient()
{
    epoll_.StopEpoll();
    if(queue_th_){
        queue_th_->join();
        delete queue_th_;
    }
}

bool RecvMsg(int fd, void* buf, size_t size) {
    char* ptr = static_cast<char*>(buf);
    size_t total = 0;

    while (total < size) {
        ssize_t n = recv(fd, ptr + total, size - total, MSG_WAITALL);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}



void SimpleClient::Connect(){
    auto sock = create_socket_connect();
    if (sock < 0){
        state_ = ClientState::ERROR;
        return ;
    }

    socket_ = sock;
    // state_ = ClientState::WAITING;
    epoll_.AddFd(sock);
    epoll_.RunEpoll();


    state_ = ClientState::HANDSHAKE;

    // отправляем сразу!
    // TODO(): load uuid
    uuid_ = generateUuid();
    ClientHiMsg msg;
    msg.uuid = uuid_;
    d("-cli handshake send " << sizeof(msg) << " uuid:" << uuid_to_string(uuid_))
    SendToSocket(reinterpret_cast<char*>(&msg), sizeof(msg));

    d("-cli handshake wait server msg!")
    // надо ли ждать сразу?
    ServerAnsHiMsg smsg;
    int size = sizeof(smsg);
    RecvMsg(socket_, (char*)&smsg, sizeof(smsg));

    d("-cli end handshake get recv! " << smsg.client_mode)
    if (smsg.client_mode == ServerAnsHiMsg::SEND){
        state_ = ClientState::WAITING;
        return;
    }
    state_ = ClientState::ERROR;
}

void SimpleClient::Disconnect(){
    state_ = ClientState::DISCONNECTED;
    epoll_.RemoveFd(socket_);
    socket_ = -1;
    epoll_.StopEpoll();
}

void SimpleClient::SendToSocket(char *data, uint32_t size){

    uint32_t net_size = htonl(static_cast<uint32_t>(size));

    iovec iov[2];
    iov[0].iov_base = &net_size;
    iov[0].iov_len = sizeof(net_size);
    iov[1].iov_base = data;
    iov[1].iov_len = size;

    msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    ssize_t sent = sendmsg(socket_, &msg, MSG_NOSIGNAL);
    if (sent != size+sizeof(size)) {
        std::cerr << socket_ << " send(" << sent << ") failed: " << strerror(errno) << std::endl;
    }
    stats_.addBytes(sent);
}

void SimpleClient::QueueAdd(char *data, int size){
    std::lock_guard<std::mutex> lock(queue_mtx_);
    queue_.push(std::make_pair(data, size));
}

void SimpleClient::QueueSend(){
    std::lock_guard<std::mutex> lock(queue_mtx_);
    while(!queue_.empty()){
        auto el = queue_.front();
        queue_.pop();
        SendToSocket(el.first, el.second);
    }
}

void SimpleClient::StartAsyncQueue()
{
    queue_th_ = new std::thread([&](){
        while (socket_ > 0){
            if (queue_.empty()){
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            QueueSend();
        }
    });
}

void SimpleClient::AddHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    dispatcher_->setHandler(type, std::move(handler));
}

// ответы от сервера (пока нету)
void SimpleClient::handleData(){
    ssize_t n;
    // это не SubEpoll, тут не нужна статистика
    // std::cout << "2handle_socket_data " << n << std::endl;
    n = recv(socket_, buffer, sizeof(buffer), 0);
    if (n > 0) {
        if (dispatcher_) {
            dispatcher_->onEvent(EventType::DataReceived, &n);
        }
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

