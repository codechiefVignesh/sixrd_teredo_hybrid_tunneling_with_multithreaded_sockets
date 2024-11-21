// teredo_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define PORT 3544
#define MAX_PACKET_SIZE 9000
#define MIN_PACKET_SIZE 64
#define STEP_SIZE 10
#define NUM_PACKETS 100

// Teredo header structure
struct teredo_header {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t client_port;
    uint32_t client_ip;
};

void test_performance(int sockfd, struct sockaddr_in6 *server_addr, const char *output_file) {
    FILE *fp = fopen(output_file, "w");
    if (!fp) {
        perror("Failed to open output file");
        return;
    }

    fprintf(fp, "PacketSize,Latency(ms),Throughput(Mbps)\n");

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    for (int size = MIN_PACKET_SIZE; size <= MAX_PACKET_SIZE; size += STEP_SIZE) {
        char *send_buffer = malloc(size + sizeof(struct teredo_header));
        char *recv_buffer = malloc(size + sizeof(struct teredo_header));
        if (!send_buffer || !recv_buffer) {
            printf("Memory allocation failed for size %d\n", size);
            continue;
        }

        // Prepare Teredo header
        struct teredo_header *header = (struct teredo_header *)send_buffer;
        header->version = 1;
        header->type = 0;
        header->length = htons(size);
        header->client_port = htons(PORT);
        header->client_ip = 0;

        // Fill the rest with test data
        memset(send_buffer + sizeof(struct teredo_header), 'A', size);

        double total_time = 0;
        long total_bytes = 0;
        int successful_packets = 0;

        printf("Testing with packet size: %d bytes\n", size);

        for (int i = 0; i < NUM_PACKETS; i++) {
            struct timeval start, end;
            gettimeofday(&start, NULL);

            ssize_t sent = sendto(sockfd, send_buffer, size + sizeof(struct teredo_header), 0,
                                (struct sockaddr *)server_addr, sizeof(*server_addr));

            if (sent > 0) {
                socklen_t server_len = sizeof(*server_addr);
                ssize_t received = recvfrom(sockfd, recv_buffer, size + sizeof(struct teredo_header), 0,
                                          (struct sockaddr *)server_addr, &server_len);

                if (received > sizeof(struct teredo_header)) {
                    gettimeofday(&end, NULL);
                    total_bytes += (sent - sizeof(struct teredo_header));
                    successful_packets++;
                    
                    double time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                                   (end.tv_usec - start.tv_usec) / 1000.0;
                    total_time += time_ms;
                }
            }
        }

        if (successful_packets > 0) {
            double avg_latency = total_time / successful_packets;
            double throughput = ((total_bytes * 8.0) / (total_time)) * 1000.0 / (1024*1024); // Mbps
            fprintf(fp, "%d,%.2f,%.2f\n", size, avg_latency, throughput);
            printf("Size: %d, Latency: %.2f ms, Throughput: %.2f Mbps\n",
                   size, avg_latency, throughput);
        } else {
            fprintf(fp, "%d,0.00,0.00\n", size);
            printf("No successful packets for size %d\n", size);
        }

        free(send_buffer);
        free(recv_buffer);
    }

    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <output_file>\n", argv[0]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in6 server_addr;

    // Create IPv6 UDP socket
    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(PORT);
    
    // Convert localhost (::1) to binary form
    if (inet_pton(AF_INET6, "::1", &server_addr.sin6_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    printf("Starting performance test...\n");
    test_performance(sockfd, &server_addr, argv[1]);

    close(sockfd);
    return 0;
}