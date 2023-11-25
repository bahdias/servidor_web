#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_THREADS 4
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 10
#define FILE_PATH "image.jpg"

// Estrutura para armazenar os argumentos da thread
typedef struct {
    int sock;
} thread_arg_t;

// Estrutura da fila de tarefas
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

void send_response(int client_socket, int status, const char *status_text, const char *content_type, const char *content, long content_length) {
    char header[1024];

    sprintf(header, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", status, status_text, content_type, content_length);
    send(client_socket, header, strlen(header), 0);
    send(client_socket, content, content_length, 0);
}

void handle_http_request(int sockfd) {
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    ssize_t n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n < 0) error("ERROR reading from socket");

    printf("Here is the message:\n%s\n", buffer);

    // Assumindo que a requisição é para "GET /image.jpg"
    char method[10], url[100], version[20];
    if (sscanf(buffer, "%s %s %s", method, url, version) == 3) {
        if (strcmp(url, "/image.jpg") == 0) {
            // Apenas para simplificar, suponha que o arquivo "image.jpg" exista e tenha 1KB
            char *file_content = "Fake image content";
            long file_size = strlen(file_content); // Deve ser o tamanho real do arquivo

            send_response(sockfd, 200, "OK", "image/jpeg", file_content, file_size);
        } else {
            // URL não reconhecida
            send_response(sockfd, 404, "Not Found", "text/html", "404 Not Found", strlen("404 Not Found"));
        }
    } else {
        // Requisição HTTP inválida
        send_response(sockfd, 400, "Bad Request", "text/html", "400 Bad Request", strlen("400 Bad Request"));
    }
}

void *worker_thread(void *arg) {
    while (1) {
        int sock = queue_pop(&queue);
        handle_http_request(sock);
        close(sock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t threads[MAX_THREADS];

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    queue_init(&queue);

    for (int i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL)) {
            error("ERROR creating thread");
        }
    }

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        queue_push(&queue, newsockfd);
    }

    close(sockfd);
    return 0;
}
