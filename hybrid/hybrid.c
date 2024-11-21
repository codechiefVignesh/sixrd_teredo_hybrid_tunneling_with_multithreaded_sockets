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

#define BUFFER_SIZE 9000
#define SIXRD_PORT 8001
#define TEREDO_PORT 8002
#define RECEIVER_PORT 8003

// Structure for packet data
struct packet {
    char data[BUFFER_SIZE];
    int length;
    int type;  // 0 for IPv4, 1 for IPv6
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

        if (n < 0) continue;

        // Get the IP of the client
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // Calculate latency
        long latency = calculate_latency(start, end);

        // Calculate throughput in KBps
        double throughput = (pkt.length / (latency / 1e6)) / 1024.0;  // in KBps

        packet_count++;
        printf("6RD Server received from %s: %s (Latency: %ld us, Throughput: %.2f KBps)\n", 
               client_ip, pkt.data, latency, throughput);
        log_metrics("metrics.txt", packet_count, latency, throughput);

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

        if (n < 0) continue;

        // Calculate latency
        long latency = calculate_latency(start, end);

        // Calculate throughput in KBps
        double throughput = (pkt.length / (latency / 1e6)) / 1024.0;  // in KBps

        packet_count++;
        printf("Teredo Server received and forwarding: %s (Latency: %ld us, Throughput: %.2f KBps)\n", 
               pkt.data, latency, throughput);
        log_metrics("metrics.txt", packet_count, latency, throughput);

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <mode>\n", argv[0]);
        printf("Modes:\n");
        printf("1 - Run servers\n");
        printf("2 - Run sender\n");
        printf("3 - Run receiver\n");
        return 1;
    }

    int mode = atoi(argv[1]);

    if (mode == 1) {
        // Run both servers
        pthread_t sixrd_thread, teredo_thread;
        
        if (pthread_create(&sixrd_thread, NULL, sixrd_server, NULL) != 0)
            handle_error("Failed to create 6RD thread");
            
        if (pthread_create(&teredo_thread, NULL, teredo_server, NULL) != 0)
            handle_error("Failed to create Teredo thread");

        pthread_join(sixrd_thread, NULL);
        pthread_join(teredo_thread, NULL);
    }
    else if (mode == 2) {
        // Sender
        int sockfd;
        struct sockaddr_in sixrd_addr;
        struct packet pkt;

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) 
            handle_error("Sender socket creation failed");

        memset(&sixrd_addr, 0, sizeof(sixrd_addr));
        sixrd_addr.sin_family = AF_INET;
        sixrd_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        sixrd_addr.sin_port = htons(SIXRD_PORT);

        printf("Sender started. Type messages to send (Ctrl+C to quit):\n");

        // int size = 50;
        // while (size<=9000) {
        //     // Get input from user
        //     printf("Enter message: ");
        //     fgets(pkt.data, BUFFER_SIZE, stdin);
        //     pkt.data[strcspn(pkt.data, "\n")] = 0;  // Remove newline
        //     pkt.length = strlen(pkt.data);
        //     pkt.type = 1;  // IPv6

        
        //     // memset(pkt.data, 'A', size);
        //     // pkt.length = strlen(pkt.data);
        //     // pkt.type = 1;
        //     // Send to 6RD server
        //     sendto(sockfd, &pkt, size, 0, 
        //           (struct sockaddr *)&sixrd_addr, sizeof(sixrd_addr));
        //     size+=50;
        // }
        int size = 50;
    while (size <= 9000) {
    // Fill the packet with 'A' for the current size
    memset(pkt.data, 'A', size);  // Fill pkt.data with 'A' characters
    
    pkt.length = size;  // Set the length of the packet based on current size
    pkt.type = 1;  // IPv6

    // Send the packet to the 6RD server
    sendto(sockfd, &pkt, size, 0, (struct sockaddr *)&sixrd_addr, sizeof(sixrd_addr));

    size += 10;  // Increase the size by 10 bytes
}

    }
    else if (mode == 3) {
        // Receiver
        int sockfd;
        struct sockaddr_in receiver_addr;
        struct packet pkt;

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) 
            handle_error("Receiver socket creation failed");

        memset(&receiver_addr, 0, sizeof(receiver_addr));
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr.s_addr = INADDR_ANY;
        receiver_addr.sin_port = htons(RECEIVER_PORT);

        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
            handle_error("Receiver setsockopt failed");

        if (bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0)
            handle_error("Receiver Bind failed");

        printf("Receiver is waiting for packets on port %d...\n", RECEIVER_PORT);

        while (1) {
            int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, NULL, NULL);
            if (n < 0) continue;
            printf("Received: %s\n", pkt.data);
        }
    }

    return 0;
}
