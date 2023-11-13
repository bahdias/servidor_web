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
#define QUEUE_SIZE 10 // Tamanho máximo da fila

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

void handle_http_request(int sockfd) {
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    ssize_t n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n < 0) error("ERROR reading from socket");

    printf("Here is the message:\n%s\n", buffer);

    if (strncmp(buffer, "GET ", 4) == 0) {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html>"
            "<head><title>Server Response</title></head>"
            "<body><h1>Hello, World!</h1></body>"
            "</html>";

        n = write(sockfd, response, strlen(response));
        if (n < 0) error("ERROR writing to socket");
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

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    // Inicialização da fila
    queue_init(&queue);

    // Criação das threads trabalhadoras
    for (int i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL)) {
            error("ERROR creating thread");
        }
    }

    while (1) {
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        // Adicionando o novo socket à fila
        queue_push(&queue, newsockfd);
    }

    // Fechar o socket do servidor (não alcançado neste exemplo)
    close(sockfd);
    return 0; // Nem este será alcançado
}
