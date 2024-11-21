#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define SIXRD_PORT 8001
#define TEREDO_PORT 8002
#define RECEIVER_PORT 8003
#define PACKET_LIMIT 9000

// Structure for packet data
struct packet {
    char data[BUFFER_SIZE];
    int length;
    int type; // 0 for IPv4, 1 for IPv6
};

// Function to handle errors
void handle_error(const char *message) {
    perror(message);
    exit(1);
}

// Function to calculate time difference in microseconds
long calculate_latency(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_usec - start.tv_usec);
}

// Function to log metrics
void log_metrics(const char *filename, int packet_count, long latency, double throughput) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Failed to open metrics file");
        return;
    }
    fprintf(file, "%d,%ld,%.2f\n", packet_count, latency, throughput);
    fclose(file);
}

// Simulated 6RD Server
void *sixrd_server(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr;
    int packet_count = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        handle_error("6RD Socket creation failed");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SIXRD_PORT);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        handle_error("6RD setsockopt failed");

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        handle_error("6RD Bind failed");

    printf("6RD Server is running on port %d...\n", SIXRD_PORT);

    while (1) {
        struct packet pkt;
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        struct timeval start, end;

        // Receive data
        gettimeofday(&start, NULL);
        int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&client_addr, &len);
        gettimeofday(&end, NULL);

        if (n < 0)
            continue;

        // Calculate latency
        long latency = calculate_latency(start, end);

        // Calculate throughput in KBps
        double throughput = (pkt.length / (latency / 1e6)) / 1024.0; // in KBps

        packet_count++;
        printf("6RD Server received: %s (Latency: %ld us, Throughput: %.2f KBps)\n",
               pkt.data, latency, throughput);
        log_metrics("metrics_6rd.csv", packet_count, latency, throughput);

        // Forward to Teredo server
        struct sockaddr_in teredo_addr;
        memset(&teredo_addr, 0, sizeof(teredo_addr));
        teredo_addr.sin_family = AF_INET;
        teredo_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        teredo_addr.sin_port = htons(TEREDO_PORT);

        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&teredo_addr, sizeof(teredo_addr));
    }
    close(sockfd);
    return NULL;
}

// Simulated Teredo Server
void *teredo_server(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr;
    int packet_count = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        handle_error("Teredo Socket creation failed");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(TEREDO_PORT);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        handle_error("Teredo setsockopt failed");

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        handle_error("Teredo Bind failed");

    printf("Teredo Server is running on port %d...\n", TEREDO_PORT);

    while (1) {
        struct packet pkt;
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        struct timeval start, end;

        // Receive data
        gettimeofday(&start, NULL);
        int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&client_addr, &len);
        gettimeofday(&end, NULL);

        if (n < 0)
            continue;

        // Calculate latency
        long latency = calculate_latency(start, end);

        // Calculate throughput in KBps
        double throughput = (pkt.length / (latency / 1e6)) / 1024.0; // in KBps

        packet_count++;
        printf("Teredo Server received: %s (Latency: %ld us, Throughput: %.2f KBps)\n",
               pkt.data, latency, throughput);
        log_metrics("metrics_teredo.csv", packet_count, latency, throughput);

        // Forward to receiver
        struct sockaddr_in receiver_addr;
        memset(&receiver_addr, 0, sizeof(receiver_addr));
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        receiver_addr.sin_port = htons(RECEIVER_PORT);

        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
    }
    close(sockfd);
    return NULL;
}

// Sender Function
void sender() {
    int sockfd;
    struct sockaddr_in server_addr;
    char message[PACKET_LIMIT];
    struct timeval start, end;

    // Fill data
    memset(message, 'A', sizeof(message));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        handle_error("Socket creation failed");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SIXRD_PORT);

    int bytes_sent = 0;
    int total_packets = 0;

    for (int i = 0; i < PACKET_LIMIT; i += BUFFER_SIZE) {
        struct packet pkt;
        strncpy(pkt.data, message + i, BUFFER_SIZE);
        pkt.length = BUFFER_SIZE;

        gettimeofday(&start, NULL);
        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        gettimeofday(&end, NULL);

        long latency = calculate_latency(start, end);
        double throughput = (BUFFER_SIZE / (latency / 1e6)) / 1024.0; // KBps
        bytes_sent += BUFFER_SIZE;
        total_packets++;

        printf("Sent packet %d: Latency %ld us, Throughput %.2f KBps\n", total_packets, latency, throughput);
        log_metrics("metrics_sender.csv", total_packets, latency, throughput);
    }

    printf("Sender completed: Total bytes sent = %d\n", bytes_sent);
    close(sockfd);
}

// Receiver Function
void receiver() {
    int sockfd;
    struct sockaddr_in server_addr;
    int total_received = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        handle_error("Socket creation failed");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(RECEIVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        handle_error("Bind failed");

    printf("Receiver is waiting for packets on port %d...\n", RECEIVER_PORT);

    while (1) {
        struct packet pkt;
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&client_addr, &len);
        if (n <= 0)
            break;

        total_received += pkt.length;
        printf("Received: %s (Bytes: %d)\n", pkt.data, pkt.length);
    }

    printf("Receiver completed: Total bytes received = %d\n", total_received);
    close(sockfd);
}

// Main Function
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s sender|receiver|6rd|teredo\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "sender") == 0) {
        sender();
    } else if (strcmp(argv[1], "receiver") == 0) {
        receiver();
    } else if (strcmp(argv[1], "6rd") == 0) {
        pthread_t sixrd_thread;
        pthread_create(&sixrd_thread, NULL, sixrd_server, NULL);
        pthread_join(sixrd_thread, NULL);
    } else if (strcmp(argv[1], "teredo") == 0) {
        pthread_t teredo_thread;
        pthread_create(&teredo_thread, NULL, teredo_server, NULL);
        pthread_join(teredo_thread, NULL);
    } else {
        fprintf(stderr, "Invalid mode. Choose sender, receiver, 6rd, or teredo.\n");
        exit(1);
    }

    return 0;
}
