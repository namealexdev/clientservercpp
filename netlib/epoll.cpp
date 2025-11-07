#include "epoll.h"

bool BaseEpoll::add_fd(int fd)
{
    const uint32_t events = EPOLLIN | EPOLLRDHUP;
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
        thLoop_->join();
        thLoop_.reset();
    }
    thLoop_ = std::make_unique<std::thread>([this](){execLoop();});
}

void BaseEpoll::stop(){
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

            auto fd = events[i].data.fd;
            auto ev = events[i].events;
            switch (ev & (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            case EPOLLOUT:
                // Можно писать в сокет
                break;
            case EPOLLIN:
                // Можно читать из сокета (например, при accept)
                break;
            case EPOLLIN | EPOLLOUT:
                // Можно и читать, и писать
                break;
            default:
                // Выкидываем сокет: закрытие (EPOLLHUP, EPOLLRDHUP) или ошибка (EPOLLERR, включая broken pipe)
                break;
            }

            // if (onEvent){
            //     onEvent(events[i].data.fd, events[i].events);
            // }
        }
    }
}


