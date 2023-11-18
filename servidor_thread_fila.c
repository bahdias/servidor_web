#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_TRDS 4
#define BUF_SIZE 4096
#define Q_SIZE 10 

typedef struct {
    int sock;
} t_arg;

typedef struct {
    int socks[Q_SIZE];
    int start;
    int end;
    int num;
    pthread_mutex_t mut;
    pthread_cond_t notEmp;
    pthread_cond_t notFull;
} t_queue;

t_queue q;

void boo(const char *msg) {
    perror(msg);
    exit(1);
}

void initQ(t_queue *que) {
    que->start = 0;
    que->end = -1;
    que->num = 0;
    pthread_mutex_init(&que->mut, NULL);
    pthread_cond_init(&que->notEmp, NULL);
    pthread_cond_init(&que->notFull, NULL);
}

void addQ(t_queue *que, int sock) {
    pthread_mutex_lock(&que->mut);
    while (que->num == Q_SIZE) {
        pthread_cond_wait(&que->notFull, &que->mut);
    }
    que->end = (que->end + 1) % Q_SIZE;
    que->socks[que->end] = sock;
    que->num++;
    pthread_cond_signal(&que->notEmp);
    pthread_mutex_unlock(&que->mut);
}

int getQ(t_queue *que) {
    pthread_mutex_lock(&que->mut);
    while (que->num == 0) {
        pthread_cond_wait(&que->notEmp, &que->mut);
    }
    int sock = que->socks[que->start];
    que->start = (que->start + 1) % Q_SIZE;
    que->num--;
    pthread_cond_signal(&que->notFull);
    pthread_mutex_unlock(&que->mut);
    return sock;
}

void procReq(int sock) {
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    ssize_t n = read(sock, buf, BUF_SIZE - 1);
    if (n < 0) boo("ERROR reading from socket");

    buf[n] = '\0'; 
    printf("Got this request:\n%s\n", buf);

    char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><head><title>Hey</title></head><body><h1>Hi!</h1></body></html>";

    n = write(sock, resp, strlen(resp));
    if (n < 0) boo("ERROR writing to socket");
}

void *thdFunc(void *arg) {
    while (1) {
        int sock = getQ(&q);
        procReq(sock);
        close(sock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int sock, newsock, portno;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t trds[MAX_TRDS];

    if (argc < 2) {
        fprintf(stderr, "Need port number\n");
        exit(1);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) boo("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) boo("ERROR on binding");

    listen(sock, 5);

    initQ(&q);

    for (int i = 0; i < MAX_TRDS; i++) {
        if (pthread_create(&trds[i], NULL, thdFunc, NULL)) boo("ERROR creating thread");
    }

    while (1) {
        newsock = accept(sock, (struct sockaddr *)&cli_addr, &portno);
        if (newsock < 0) boo("ERROR on accept");

        addQ(&q, newsock);
    }

    close(sock);
    return 0;
}
