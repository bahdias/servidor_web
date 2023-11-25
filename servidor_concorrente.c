#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10
#define FILE_PATH "image.jpg"  // Caminho para o arquivo da imagem

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void send_response(int client_socket, int status, const char *status_text, const char *content_type, const char *content, long content_length) {
    char response_header[1024];
    sprintf(response_header, "HTTP/1.1 %d %s\r\n"
                             "Accept-Ranges: bytes\r\n"
                             "Content-Length: %ld\r\n"
                             "Content-Type: %s\r\n\r\n",
                             status, status_text, content_length, content_type);
    send(client_socket, response_header, strlen(response_header), 0);
    if (content_length > 0) {
        send(client_socket, content, content_length, 0);
    }
}

int process_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    ssize_t n = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (n < 0) {
        perror("ERROR reading from socket");
        return -1;
    }

    if (n == 0) {
        return -1;
    }

    buffer[n] = '\0'; 
    printf("Request received:\n%s\n", buffer);

    char method[10], url[100], version[20];
    if (sscanf(buffer, "%s %s %s", method, url, version) == 3) {
        if (strcmp(url, "/image.jpg") == 0) {
            FILE *file = fopen(FILE_PATH, "rb");
            if (file) {
                fseek(file, 0, SEEK_END);
                long file_size = ftell(file);
                rewind(file);

                char *file_content = (char *)malloc(file_size);
                if (!file_content) {
                    error("Failed to allocate memory for file content");
                }

                fread(file_content, 1, file_size, file);
                fclose(file);

                send_response(client_socket, 200, "OK", "image/jpeg", file_content, file_size);

                free(file_content);
            } else {
                send_response(client_socket, 404, "Not Found", "text/html", "404 Not Found", 0);
            }
        } else {
            send_response(client_socket, 404, "Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>", 0);
        }
    } else {
        error("Invalid HTTP request");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int server_socket, new_socket, port_number, client_len;
    struct sockaddr_in server_addr, client_addr;
    int client_sockets[MAX_CLIENTS] = {0};
    fd_set read_fds;
    int max_sd, sd;
    int activity;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        error("ERROR opening socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    port_number = atoi(argv[1]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_number);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("ERROR on binding");
    }

    listen(server_socket, 5);
    printf("Server started on port %d\n", port_number);

    client_len = sizeof(client_addr);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        max_sd = server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &read_fds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            error("ERROR in select");
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            new_socket = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
            if (new_socket < 0) {
                error("ERROR on accept");
            }

            printf("New connection: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets at index %d\n", i);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &read_fds)) {
                if (process_request(sd) == -1) {
                    getpeername(sd, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
                    printf("Client disconnected, ip %s, port %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
