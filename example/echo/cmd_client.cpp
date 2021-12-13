#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"

// Note: Check for SIGPIPE and other socket related signals

int main(int argc, char** argv) {
    // Check arguments
    if (argc != 2) {
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port}" << std::endl;
        return EXIT_FAILURE;
    }

    auto port = expectInt(argv[2], "port is an invalid number.");
    if (port < 0 || port > 65535) {
        std::cerr << "port should be between 0 and 65535." << std::endl;
        return EXIT_FAILURE;
    }
    // Address and host info
    sockaddr_in server_addr;
    hostent* server;

    // Create a socket & get the file descriptor
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    // Check If the socket is created
    if (sock_fd < 0) {
        std::cerr << "[ERROR] Socket cannot be created!\n";
        return EXIT_FAILURE;
    }

    std::cout << "[INFO] Socket has been created.\n";

    // Get host information by name
    // gethostbyname is not thread-safe, checkout getaddrinfo
    server = gethostbyname(argv[1]);
    if (!server) {
        std::cerr << "[ERROR] No such host!\n";
        return EXIT_FAILURE;
    }
    std::cout << "[INFO] Hostname is found.\n";

    // Fill address fields before try to connect
    std::memset((char*)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Check if there is an address of the host
    if (server->h_addr_list[0])
        std::memcpy((char*)server->h_addr_list[0], (char*)&server_addr.sin_addr, server->h_length);
    else {
        std::cerr << "[ERROR] There is no a valid address for that hostname!\n";
        return EXIT_FAILURE;
    }

    if (connect(sock_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection cannot be established!\n";
        return EXIT_FAILURE;
    }
    std::cout << "[INFO] Connection established.\n";

    char buf[4096] { "test!" };
    std::string temp;
    // While user input is not empty or "quit"
    // Send data to the server
    // Wait for a message/response
    // Print the response
    do {
        // Clear buffer, get user input
        std::memset(buf, 0, 4096);
        std::cout << "> ";
        std::getline(std::cin, temp, '\n');
        std::strcpy(buf, temp.c_str());

        // Check the input
        if (!strlen(buf)) {
            if (std::cin.eof()) {
                break;
            } else {
                continue;
            }
        } else if (!strcmp(buf, "quit")) {
            break;
        }

        // Send the data that buffer contains
        int bytes_send = send(sock_fd, &buf, (size_t)strlen(buf), 0);
        // Check if message sending is successful
        if (bytes_send < 0) {
            std::cerr << "[ERROR] Message cannot be sent!\n";
            break;
        }

        // Wait for a message
        int bytes_recv = recv(sock_fd, &buf, 4096, 0);

        // Check how many bytes recieved
        // If something gone wrong
        if (bytes_recv < 0) {
            std::cerr << "[ERROR] Message cannot be recieved!\n";
        }
        // If there is no data, it means server is disconnected
        else if (bytes_recv == 0) {
            std::cout << "[INFO] Server is disconnected.\n";
        }
        // If there is some bytes, print
        else {
            std::cout << "server> " << std::string(buf, 0, bytes_recv) << "\n";
        }

    } while (true);

    // Close socket
    close(sock_fd);
    std::cout << "[INFO] Socket is closed.\n";

    return EXIT_SUCCESS;
}