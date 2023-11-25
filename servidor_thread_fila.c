#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_THREADS 4
#define BUFFER_SIZE 8192
#define QUEUE_SIZE 100
#define FILE_PATH "image.jpg"

typedef struct {
    int sockets[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
} socket_queue_t;

socket_queue_t queue;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void queue_init(socket_queue_t *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
    pthread_cond_init(&q->notFull, NULL);
}

void queue_push(socket_queue_t *q, int socket) {
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->notFull, &q->lock);
    }
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->sockets[q->rear] = socket;
    q->count++;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(socket_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) {
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }
    int socket = q->sockets[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->lock);
    return socket;
}

void send_response(int client_socket, const char *header, const char *content, long content_length) {
    send(client_socket, header, strlen(header), 0);
    send(client_socket, content, content_length, 0);
}

void *worker_thread(void *arg) {
    while (1) {
        int sock = queue_pop(&queue);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(sock, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            error("ERROR reading from socket");
        } else {
            buffer[n] = '\0'; 
            printf("Here is the message:\n%s\n", buffer);

            char method[10], url[100], version[20];
            if (sscanf(buffer, "%s %s %s", method, url, version) == 3) {
                if (strcmp(url, "/image.jpg") == 0) {
                    FILE *file = fopen(FILE_PATH, "rb");
                    if (file) {
                        fseek(file, 0, SEEK_END);
                        long file_size = ftell(file);
                        fseek(file, 0, SEEK_SET);

                        char *file_content = (char *)malloc(file_size);
                        if (!file_content) {
                            error("Failed to allocate memory for file content");
                        }

                        fread(file_content, 1, file_size, file);
                        fclose(file);

                        char header[1024];
                        sprintf(header, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nContent-Length: %ld\r\nContent-Type: image/jpeg\r\n\r\n", file_size);
                        send_response(sock, header, file_content, file_size);

                        free(file_content);
                    } else {
                        error("File not found");
                    }
                } else {
                    char *response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
                    send(sock, response, strlen(response), 0);
                }
            } else {
                error("Invalid HTTP request");
            }
        }

        close(sock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int sockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t threads[MAX_THREADS];

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    queue_init(&queue);

    for (int i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL)) error("ERROR creating thread");
    }

    while (1) {
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        queue_push(&queue, newsockfd);
    }

    close(sockfd);
    return 0;
}
