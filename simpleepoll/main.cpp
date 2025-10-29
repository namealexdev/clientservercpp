#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <atomic>

class EpollServer {
private:
    int server_fd;
    int epoll_fd;
    std::atomic<bool> running{true};
    std::vector<std::thread> workers;

    static const int MAX_EVENTS = 64;
    static const int BUFFER_SIZE = 4096;

public:
    EpollServer(int port, int thread_count = 4) {
        // Create server socket
        server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (server_fd == -1) {
            throw std::runtime_error("Failed to create socket");
        }

        // Set SO_REUSEADDR
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(server_fd);
            throw std::runtime_error("Failed to set SO_REUSEADDR");
        }

        // Bind
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(server_fd);
            throw std::runtime_error("Failed to bind socket");
        }

        // Listen
        if (listen(server_fd, SOMAXCONN) < 0) {
            close(server_fd);
            throw std::runtime_error("Failed to listen");
        }

        // Create epoll instance
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            close(server_fd);
            throw std::runtime_error("Failed to create epoll");
        }

        // Add server socket to epoll
        epoll_event event{};
        event.events = EPOLLIN | EPOLLET; // Edge-triggered
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
            close(server_fd);
            close(epoll_fd);
            throw std::runtime_error("Failed to add server fd to epoll");
        }

        std::cout << "Server started on port " << port << " with " << thread_count << " threads\n";

        // Start worker threads
        for (int i = 0; i < thread_count; ++i) {
            workers.emplace_back(&EpollServer::worker_thread, this, i);
        }
    }

    ~EpollServer() {
        stop();
    }

    void stop() {
        if (running.exchange(false)) {
            std::cout << "Stopping server...\n";

            // Close server socket to wake up epoll_wait
            if (server_fd != -1) {
                close(server_fd);
                server_fd = -1;
            }

            // Close epoll to wake up threads
            if (epoll_fd != -1) {
                close(epoll_fd);
                epoll_fd = -1;
            }

            // Join all threads
            for (auto& thread : workers) {
                if (thread.joinable()) {
                    thread.join();
                }
            }

            std::cout << "Server stopped\n";
        }
    }

private:
    void worker_thread(int thread_id) {
        std::cout << "Worker thread " << thread_id << " started\n";

        epoll_event events[MAX_EVENTS];
        char buffer[BUFFER_SIZE];

        while (running) {
            int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 100); // 100ms timeout

            if (num_events == -1) {
                if (errno != EINTR) {
                    perror("epoll_wait");
                }
                continue;
            }

            for (int i = 0; i < num_events && running; ++i) {
                if (events[i].data.fd == server_fd) {
                    // New connection
                    accept_connections();
                } else {
                    // Data from client
                    handle_client(events[i].data.fd, buffer);
                }
            }
        }

        std::cout << "Worker thread " << thread_id << " stopped\n";
    }

    void accept_connections() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        while (running) {
            int client_fd = accept4(server_fd, (sockaddr*)&client_addr, &client_len,
                                    SOCK_NONBLOCK);

            if (client_fd == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("accept4");
                }
                break;
            }

            // Add client to epoll
            epoll_event event{};
            event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; // Edge-triggered + hangup detection
            event.data.fd = client_fd;

            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                perror("epoll_ctl: client_fd");
                close(client_fd);
                continue;
            }

            std::cout << "New connection accepted, fd: " << client_fd << "\n";
        }
    }

    void handle_client(int client_fd, char* buffer) {
        while (running) {
            ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

            if (bytes_read > 0) {
                // Data received successfully
                // Do nothing with data as requested
                continue; // Continue reading in edge-triggered mode
            }
            else if (bytes_read == 0) {
                // Client disconnected
                std::cout << "Client disconnected, fd: " << client_fd << "\n";
                close(client_fd);
                break;
            }
            else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more data to read (edge-triggered behavior)
                    break;
                } else {
                    // Error
                    perror("recv");
                    close(client_fd);
                    break;
                }
            }
        }
    }
};
#include <string.h>
int main(int argc, char* argv[]) {
    // if (argc == 1){
        // printf("[port]");return 1;
    // }

    // printf("%s", std::string(argv[2]).c_str());
    try {
        int port = 5202;
        // port = std::stoi(std::string(argv[2]));
        EpollServer server(port, 4);

        std::cout << "Server running. Press Enter to stop...\n";
        std::cin.get();

        server.stop();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
