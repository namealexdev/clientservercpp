#ifndef CONST_H
#define CONST_H

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <queue>
#include <mutex>
#include <arpa/inet.h>
#include <random>

#include <thread>
// #include <threadpool.h>

using std::string;

#include "utils.h"
#include "libinclude/stats.h"

#define BUF_READ_SIZE 65536
#define EPOLL_MAX_EVENTS 64
#define EPOLL_TIMEOUT 100

constexpr int PACKET_HEADER_SIZE = 4;
constexpr uint32_t PACKET_MAX_PAYLOAD_SIZE = 16 * 1024 * 1024; // 16MB защита
constexpr size_t PACKET_MAX_SIZE = PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD_SIZE;

#define d(x) std::cout << x << " \t(" << __FUNCTION__ << " " << __LINE__ << ")\n";
// #define d(x)

#endif // CONST_H
