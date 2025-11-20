#include "epoll.h"

bool BaseEpoll::AddFd(int fd)
{
    /*
     * IN - можно читать (recv)
     * OUT - можно писать (send) по умолчанию всегда, обычно постоянно НЕ включают
     * ET - edge-triggered (событие только при изменении состояния)
     * ERR - ошибки сокета
     * RDHUP - remote hang up (удаленная сторона закрыла соединение)
     * HUP - hang up (полное закрытие соединения)
     */
    // EPOLLOUT c EPOLLET не используем, иначе надо писать до последнего иначе OUT не приходит
    // const uint32_t ev_client = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;
    // const uint32_t ev_server = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET | EPOLLOUT;

    const uint32_t events = EPOLLIN | EPOLLRDHUP | EPOLLERR;//EPOLLOUT
    epoll_event ev{.events = events, .data{.fd = fd}};
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // throw std::runtime_error("epoll_ctl add");
        std::cout << "fail epoll_ctl add " << fd << " error: " << strerror(errno)<< std::endl;
        return false;
    }
    // d("addfd " << fd)
    return true;
}

void BaseEpoll::EnableWriteEvents(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = fd;
    epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
}

void BaseEpoll::DisableWriteEvents(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = fd;
    epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
}

void BaseEpoll::RemoveFd(int fd)
{
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void BaseEpoll::RunEpoll(bool connectInLoop/* = false*/){
    d("RunEpoll reconnect:" << connectInLoop)
    if (thLoop_){
        StopEpoll();
        thLoop_->join();
        thLoop_.reset();
    }
    thLoop_ = std::make_unique<std::thread>([&, connectInLoop](){
        // d("start th " << connectInLoop)
        if (connectInLoop && on_reconnect_){
            // d("reconnect enable")
            on_reconnect_();
        }
        ExecLoop();
    });
}

void BaseEpoll::StopEpoll(){
    need_stop_ = true;
}

BaseEpoll::BaseEpoll(){
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ == -1) throw std::runtime_error("epoll_create1");
}

BaseEpoll::~BaseEpoll(){
    if (epfd_ >= 0) {
        close(epfd_);
    }
    if (thLoop_){
        StopEpoll();
        thLoop_->join();
        thLoop_.reset();
    }
}

void BaseEpoll::ExecLoop()
{
    static epoll_event events[EPOLL_MAX_EVENTS];

    while (!need_stop_) {
        int nfds = epoll_wait(epfd_, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            d("epoll timeout")
            if (errno == EINTR) continue;
            throw std::runtime_error("epoll_wait");
        }

        for (int i = 0; i < nfds; ++i) {

            auto fd = events[i].data.fd;
            auto ev = events[i].events;

            // d(" " << fd << " " << std::hex << ev << std::dec)

            if (ev & (EPOLLHUP | EPOLLRDHUP)) {
                if (ev & EPOLLERR) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
                    if (error) {
                        d("Socket error on fd " << fd << ": " << strerror(error));
                    }
                }
                if (on_hangup_) on_hangup_(fd);
                continue;
            } else {  // Только если НЕ было HUP/RDHUP
                if (ev & EPOLLIN) {
                    if (on_read_) on_read_(fd);
                }
                if (ev & EPOLLOUT) {
                    if (on_write_) on_write_(fd);
                }
                continue;
            }

            // Проверка на неизвестные события (если не попало ни в одно условие)
            if (!(ev & (EPOLLHUP | EPOLLRDHUP | EPOLLIN | EPOLLOUT))) {
                d("Unknown event: 0x" << std::hex << ev << " on fd " << fd << std::dec);
            }

        }
    }
    d("STOP ExecLoop")
}


