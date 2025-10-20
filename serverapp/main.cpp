// server.cpp
#include <netlib.h>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [server_port] (filename)\n"
              << "  server_port  - Tcp listen port number (default: 12345)\n"
              << "  filename     - Config file (optional, if need write to file)\n"
              << "Examples:\n"
              << "  " << program_name << " 12345\n"
              << "  " << program_name << " 12345 data.txt\n";
}

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    ServerConfig conf;
    int count = 1;
    try {
        if (argc > count) conf.port = std::stoi(argv[count++]);
        if (argc > count) conf.filename = string(argv[count++]);

        if (conf.port < 1 || conf.port > 65535) {
            throw std::out_of_range("Port must be between 1 and 65535");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        return 1;
    }

    INetworkFactory* factory = new SinglethreadFactory();
    IServer *srv = factory->createServer(conf);
    srv->start();

    return 0;
}
