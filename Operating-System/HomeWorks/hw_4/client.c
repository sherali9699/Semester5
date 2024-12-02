#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> //for sockting
#include <openssl/evp.h> //for hasing

#define PORT 8080
#define SERVER_IP "127.0.0.1" // Hard-codeded server IP
#define BUFFER_SIZE 1024

// Function to compute SHA256 hash of a file
void compute_checksum(const char *filename, unsigned char *hash_out) {
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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <file_name> <num_threads>\n", argv[0]);
        return -1;
    }

    char *file_name = argv[1];
    int num_threads = atoi(argv[2]);

    if (num_threads <= 0){
        printf("The number of threads should be greater than zero! \n");
        return -1;
    }

    int sock = 0;
    struct sockaddr_in serv_addr;

    // Creating the socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Converting IPv4 address
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // Connecting to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    // Sending file request
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "%s %d", file_name, num_threads);
    send(sock, request, strlen(request), 0);

    // Receiving the checksum (SHA-256 hash) from the server as a hex string
    char server_checksum_hex[EVP_MAX_MD_SIZE * 2 + 1]; // Doubling the size for hex representation
    if (recv(sock, server_checksum_hex, sizeof(server_checksum_hex) - 1, 0) <= 0) {
        perror("[CLIENT] Failed to receive checksum from server");
        close(sock);
        return -1;
    }

    // Null-terminating the string
    server_checksum_hex[EVP_MAX_MD_SIZE * 2] = '\0';

    // Printing the received checksum as a hex string
    printf("[CLIENT] Server checksum (SHA-256): %s\n", server_checksum_hex);

    // Converting the received hex string back to binary form for comparison
    unsigned char server_checksum[EVP_MAX_MD_SIZE];
    for (int i = 0; i < EVP_MD_size(EVP_sha256()); i++) {
        sscanf(server_checksum_hex + (i * 2), "%02hhx", &server_checksum[i]);  // Use %02hhx
    }
    // Receiving file
    char output_file[100];
    snprintf(output_file, sizeof(output_file), "received_%s", file_name);

    FILE *received_file = fopen(output_file, "wb");
    if (!received_file) {
        perror("File open error");
        close(sock);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, received_file);
    }

    printf("[CLIENT] File received and saved as %s\n", output_file);

    //flusing the outputfile bofore checking checksum
    fflush(received_file);

    // Computing checksum of the received file
    unsigned char client_checksum[EVP_MAX_MD_SIZE];
    fseek(received_file, 0, SEEK_SET); // Reset file pointer to the beginning
    compute_checksum(output_file, client_checksum);

    // Printing the checksum of the received file
    printf("[CLIENT] Client checksum (SHA-256): ");
    for (int i = 0; i < EVP_MD_size(EVP_sha256()); i++) {
        printf("%02x", client_checksum[i]);
    }
    printf("\n");


    // Comparing the checksums
    if (memcmp(server_checksum, client_checksum, EVP_MD_size(EVP_sha256())) == 0) {
        printf("[CLIENT] Checksum verification succeeded! File is intact.\n");
    } else {
        printf("[CLIENT] Checksum verification failed! File may be corrupted.\n");
    }

    fclose(received_file);
    close(sock);
    return 0;
}


//Refreces:
//https://www.geeksforgeeks.org/socket-programming-cc/
//https://stackoverflow.com/questions/5791860/beginners-socket-programming-in-c
//https://codedamn.com/news/c/how-to-do-socket-programming-in-c-c
//https://www.codequoi.com/en/sockets-and-network-programming-in-c/