// teredo_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 3544  // Standard Teredo port
#define BUFFER_SIZE 9000

// Teredo header structure
struct teredo_header {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t client_port;
    uint32_t client_ip;
};

int main() {
    int server_fd;
    struct sockaddr_in6 server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create IPv6 UDP socket
    if ((server_fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Teredo Server listening on port %d...\n", PORT);

    while (1) {
        ssize_t received = recvfrom(server_fd, buffer, BUFFER_SIZE, 0,
                                  (struct sockaddr *)&client_addr, &client_len);
        
        if (received > sizeof(struct teredo_header)) {
            struct teredo_header *header = (struct teredo_header *)buffer;
            
            // Verify Teredo header
            if (header->version == 1) {
                // Echo back the payload
                sendto(server_fd, buffer, received, 0,
                      (struct sockaddr *)&client_addr, client_len);
            }
        }
    }

    close(server_fd);
    return 0;
}