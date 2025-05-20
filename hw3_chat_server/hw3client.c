#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_LENGTH 256
#define BUFFER_SIZE 1024

volatile bool running = true;

// Function prototypes
void *receive_messages(void *socket);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <server_address> <port> <username>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[3]) >= MAX_LENGTH) {
        printf("Error: Username must be less than %d characters.\n", MAX_LENGTH);
        return EXIT_FAILURE;
    }

    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    // Send username
    if (send(client_socket, argv[3], strlen(argv[3]), 0) == -1) {
        perror("Failed to send username");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Connected to server as %s.\n", argv[3]);

    // Thread for receiving messages
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, &client_socket) != 0) {
        perror("Failed to create thread");
        close(client_socket);
        return EXIT_FAILURE;
    }

    char message[BUFFER_SIZE];
    while (running) {
        if (fgets(message, sizeof(message), stdin) == NULL) {
            printf("Error reading input\n");
            break;
        }

        message[strcspn(message, "\n")] = '\0';  // Remove trailing newline

        // Handle the !exit command
        if (strcmp(message, "!exit") == 0) {
            printf("Client exiting...\n");
            if (send(client_socket, message, strlen(message), 0) == -1) {
                perror("Failed to send exit message");
            }
            running = false;  // Signal the receiving thread to stop
            break;
        }

        // Send normal or whisper messages
        if (strlen(message) > 0 && send(client_socket, message, strlen(message), 0) == -1) {
            perror("Failed to send message");
            break;
        }
    }

    // Cleanup
    close(client_socket);
    pthread_join(receive_thread, NULL);
    return 0;
}

// Thread function for receiving messages from the server
void *receive_messages(void *socket) {
    int server_socket = *(int *)socket;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (running) {
        bytes_received = recv(server_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';  // Null-terminate the received message
            printf("%s", buffer);
        } else if (bytes_received == 0) {
            printf("Server disconnected.\n");
            break;
        } else if (bytes_received < 0 && running) {  // Only report errors if still running
            perror("Error receiving message");
            break;
        }
    }

    running = false;  // Ensure the main thread knows to exit
    pthread_exit(NULL);
}
