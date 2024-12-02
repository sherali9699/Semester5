#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h> // For SHA256

#define PORT 8080
#define BUFFER_SIZE 1024

// Thread argument structure
typedef struct {
    int client_socket;
    FILE *file;
    long start;
    long end;
    int thread_no;
} ThreadArgs;

// Mutex and condition variable
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t file_cond = PTHREAD_COND_INITIALIZER;

// Variable to indicate active thread
int active_thread = 0;

// Function to compute SHA256 hash of a file
void compute_file_hash(const char *filename, unsigned char *hash_out) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        perror("Failed to create EVP_MD_CTX");
        fclose(file);
        return;
    }

    // Initializing SHA256 context
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        perror("Failed to initialize SHA256 context");
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return;
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            perror("Failed to update hash");
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return;
        }
    }

    // Finalizing the hash
    if (EVP_DigestFinal_ex(mdctx, hash_out, NULL) != 1) {
        perror("Failed to finalize hash");
    }

    EVP_MD_CTX_free(mdctx);
    fclose(file);
}

// Thread function to send file segments
void *send_file_segment(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char buffer[BUFFER_SIZE];
    long bytes_remaining = args->end - args->start;

    pthread_mutex_lock(&file_mutex);
    while (active_thread != args->thread_no) {
        pthread_cond_wait(&file_cond, &file_mutex);
    }
    pthread_mutex_unlock(&file_mutex);

    // Sending file data in chunks
    while (bytes_remaining > 0) {
        size_t chunk_size = (bytes_remaining < BUFFER_SIZE) ? bytes_remaining : BUFFER_SIZE;

        // Critical section for file access
        pthread_mutex_lock(&file_mutex);
        fseek(args->file, args->start, SEEK_SET);
        size_t bytes_read = fread(buffer, 1, chunk_size, args->file);
        pthread_mutex_unlock(&file_mutex);

        if (bytes_read == 0) {
            perror("[SERVER] File read error");
            break;
        }

        // Sending data to client
        size_t bytes_sent = send(args->client_socket, buffer, bytes_read, 0);
        if (bytes_sent <= 0) {
            perror("[SERVER] Error sending data");
            break;
        }
        // printf("[SERVER] Thread %d: Sent data.\n", args->thread_no);

        args->start += bytes_read;
        bytes_remaining -= bytes_read;
    }

    // Signaling next thread
    pthread_mutex_lock(&file_mutex);
    active_thread++;
    pthread_cond_broadcast(&file_cond);
    pthread_mutex_unlock(&file_mutex);

    free(args);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Creating the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[SERVER] Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configuring the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Binding the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[SERVER] Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listening for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("[SERVER] Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accepting a new client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("[SERVER] Accept failed");
            continue;
        }

        // Receiving file request and number of threads from the client
        char request[BUFFER_SIZE] = {0};
        if (recv(client_socket, request, sizeof(request), 0) <= 0) {
            perror("[SERVER] Failed to receive file request");
            close(client_socket);
            continue;
        }

        char file_name[128];
        int num_threads;
        sscanf(request, "%s %d", file_name, &num_threads);

        // Validating the number of threads
        if (num_threads <= 0) {
            printf("[SERVER] Invalid thread count: %d", num_threads);
            return -1;
        }

        // Opening the requested file
        FILE *file = fopen(file_name, "rb");
        if (!file) {
            perror("[SERVER] File not found");
            close(client_socket);
            continue;
        }

        // Determining the file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        rewind(file);

        unsigned char file_hash[EVP_MAX_MD_SIZE];
        compute_file_hash(file_name, file_hash);

        // Converting the hash to a hex string for transmission
        char hex_hash[EVP_MAX_MD_SIZE * 2 + 1]; // +1 for null-terminator
        for (int i = 0; i < EVP_MD_size(EVP_sha256()); i++) {
            sprintf(hex_hash + (i * 2), "%02x", file_hash[i]);
        }
        hex_hash[EVP_MD_size(EVP_sha256()) * 2] = '\0'; // Null-terminate the string


        // Sending the hash as a string to the client
        if (send(client_socket, hex_hash, strlen(hex_hash), 0) <= 0) {
            perror("[SERVER] Failed to send file hash");
            fclose(file);
            close(client_socket);
            continue;
        }

        // Reseting active thread index
        active_thread = 0;

        // Calculating segment size
        long segment_size = file_size / num_threads;

        // Creating threads to handle file transfer
        pthread_t threads[num_threads];
        for (int i = 0; i < num_threads; i++) {
            long start = i * segment_size;
            long end = (i == num_threads - 1) ? file_size : start + segment_size;

            ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
            args->client_socket = client_socket;
            args->file = file;
            args->start = start;
            args->end = end;
            args->thread_no = i;

            if (pthread_create(&threads[i], NULL, send_file_segment, args) != 0) {
                perror("[SERVER] Thread creation failed");
                free(args);
            }
        }

        // Waiting for all threads to complete
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        // Closing file and client socket
        fclose(file);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}


//Refreces:
//https://www.geeksforgeeks.org/socket-programming-cc/
//https://stackoverflow.com/questions/5791860/beginners-socket-programming-in-c
//https://codedamn.com/news/c/how-to-do-socket-programming-in-c-c
//https://www.codequoi.com/en/sockets-and-network-programming-in-c/