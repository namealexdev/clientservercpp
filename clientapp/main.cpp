// client.cpp

#include <netlib.h>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [server_host] [server_port] (host)\n"
              << "  server_host  - Tcp connect host (default: 127.0.0.1)\n"
              << "  server_port  - Tcp connect port number (default: 12345)\n"
              << "  host         - ip adress from send data (optional, if need bind to interface)\n"
              << "Examples:\n"
              << "  " << program_name << " \"127.0.0.1\" 12345\n";
}

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    ClientConfig conf;
    int count = 1;

    try {
        if (argc > count) conf.server_ip = string(argv[count++]);
        if (argc > count) conf.server_port = std::stoi(argv[count++]);
        if (argc > count) conf.host = string(argv[count++]);

        if (conf.server_port < 1 || conf.server_port > 65535) {
            throw std::out_of_range("Port must be between 1 and 65535");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        return 1;
    }

    INetworkFactory* factory = new SinglethreadFactory();
    IClient* cli = factory->createClient(conf);
    cli->start();

    return 0;
}
