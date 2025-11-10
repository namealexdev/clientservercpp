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

#define BUF_SIZE 65536
#define MAX_EVENTS 64
#define EPOLL_TIMEOUT 100

#define d(x) std::cout << x << " \t(" << __FUNCTION__ << " " << __LINE__ << ")" << std::endl;


#endif // CONST_H
