#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

#define MAX_CLIENTS 16
#define MAX_LENGTH 256
#define BUFFER_SIZE 1024

// Global variables
atomic_int running = 1;             // Controls the server's running state
atomic_int shutting_down = 0;       // Ensures shutdown is triggered only once

typedef struct {
    int socket;
    char name[MAX_LENGTH];
    char ip[INET_ADDRSTRLEN];
    int port;
    pthread_t thread;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void *handle_client(void *arg);
void broadcast_message(const char *message, int exclude_socket);
void send_whisper(const char *message, const char *target_name, const char *sender_name);
void cleanup_clients();
void sigint_handler(int sig);

// Main Function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_socket;
    struct sockaddr_in server_addr;

    // Signal handler for Ctrl+C
    signal(SIGINT, sigint_handler);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %s...\n", argv[1]);

    // Main loop to accept and handle clients
    while (atomic_load(&running)) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (!atomic_load(&running)) break; // Exit if the server is shutting down
        if (client_socket == -1) {
            if (atomic_load(&running)) perror("Accept failed");
            continue;
        }

        // Allocate memory for the new client
        Client *new_client = (Client *)malloc(sizeof(Client));
        new_client->socket = client_socket;
        inet_ntop(AF_INET, &client_addr.sin_addr, new_client->ip, sizeof(new_client->ip));
        new_client->port = ntohs(client_addr.sin_port);

        // Receive client's username
        if (recv(client_socket, new_client->name, MAX_LENGTH, 0) <= 0) {
            perror("Failed to receive username");
            close(client_socket);
            free(new_client);
            continue;
        }
        new_client->name[strcspn(new_client->name, "\n")] = '\0';  // Remove newline character

        // Add client to the list
        pthread_mutex_lock(&clients_mutex);
        clients[client_count++] = *new_client;
        pthread_mutex_unlock(&clients_mutex);

        // Notify of connection
        printf("%s connected from %s using port %d\n", new_client->name, new_client->ip, new_client->port);
        char join_message[BUFFER_SIZE];
        snprintf(join_message, sizeof(join_message), "%s has joined the chat\n", new_client->name);
        broadcast_message(join_message, -1);

        // Create a thread to handle the new client
        pthread_create(&new_client->thread, NULL, handle_client, (void *)new_client);
    }

    // Ensure cleanup is performed
    cleanup_clients();
    close(server_socket);

    // Final shutdown message
    printf("Server shut down successfully.\n");
    fflush(stdout);

    return 0;
}

// Handle Ctrl+C to stop the server
volatile sig_atomic_t shutdown_triggered = 0;

void sigint_handler(int sig) {
    if (!shutdown_triggered) {
        shutdown_triggered = 1;
        printf("\nInterrupt received. Shutting down server...\n");
        fflush(stdout);  // Ensure the message is displayed immediately
        atomic_store(&running, 0);  // Stop the main server loop
    }
}


// Broadcast a message to all clients except the excluded socket
void broadcast_message(const char *message, int exclude_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != exclude_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send a whisper message to a specific client
void send_whisper(const char *message, const char *target_name, const char *sender_name) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, target_name) == 0) {
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, sizeof(formatted_message), "(Whisper from %s): %s\n", sender_name, message);
            send(clients[i].socket, formatted_message, strlen(formatted_message), 0);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Cleanup all clients during server shutdown
void cleanup_clients() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        // Notify client about server shutdown
        send(clients[i].socket, "Server is shutting down...\n", 28, 0);
        
        // Close the client socket
        close(clients[i].socket);

        // Wait for the client thread to finish
        pthread_join(clients[i].thread, NULL);
    }
    client_count = 0;
    pthread_mutex_unlock(&clients_mutex);
}


// Thread for handling a single client
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(client->socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  // Null-terminate the received message

        // Handle special message "!exit"
        if (strcmp(buffer, "!exit") == 0) {
            printf("Client %s exiting...\n", client->name);

            // Notify all clients about the disconnection
            char disconnect_message[BUFFER_SIZE];
            snprintf(disconnect_message, sizeof(disconnect_message), "%s has left the chat\n", client->name);
            broadcast_message(disconnect_message, -1);

            // Remove client from the list
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++) {
                if (clients[i].socket == client->socket) {
                    // Shift remaining clients
                    for (int j = i; j < client_count - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    client_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            break;  // Exit the loop to clean up and close socket
        }

        // Check for whisper messages
        if (buffer[0] == '@') {
            char *target_name = strtok(buffer + 1, " ");
            char *whisper_message = strtok(NULL, "\0");
            if (target_name && whisper_message) {
                send_whisper(whisper_message, target_name, client->name);
            } else {
                char error_message[] = "Invalid whisper format. Use @username message.\n";
                send(client->socket, error_message, strlen(error_message), 0);
            }
            continue;
        }

        // Normal message
        char formatted_message[BUFFER_SIZE];
        snprintf(formatted_message, sizeof(formatted_message), "%s: %s\n", client->name, buffer);
        broadcast_message(formatted_message, client->socket);
    }

    if (bytes_received <= 0) {
        printf("Client %s disconnected unexpectedly\n", client->name);

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket == client->socket) {
                char disconnect_message[BUFFER_SIZE];
                snprintf(disconnect_message, sizeof(disconnect_message), "%s disconnected\n", clients[i].name);
                broadcast_message(disconnect_message, -1);

                // Shift remaining clients
                for (int j = i; j < client_count - 1; j++) {
                    clients[j] = clients[j + 1];
                }
                client_count--;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    close(client->socket);
    free(client);
    pthread_exit(NULL);
}
