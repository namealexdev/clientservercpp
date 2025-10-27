#ifndef UTILS_H
#define UTILS_H

//для тестов
#include <netinet/tcp.h>

#include <fcntl.h>
const int BUFFER = 1 * 1024 * 1024; // 1 MiB // 65KiB 65536
const int MAX_CONNECTIONS = 1000;

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

ssize_t read_from_stdin(void *buf, size_t maxlen);
int write_to_stdout(const char* buffer, size_t size);
int create_socket(bool islisten, const std::string& host, int port);

#endif // UTILS_H
