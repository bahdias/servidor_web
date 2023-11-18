#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>

#define BUF_SIZE 4096
#define MAX_USERS 10

void oopsie(const char *msg) {
    perror(msg);
    exit(1);
}

int processRequest(int sock) {
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    ssize_t n = read(sock, buf, BUF_SIZE - 1);
    if (n < 0) {
        perror("Oops, can't read from socket");
        return -1;
    }

    if (n == 0) {
        return -1;
    }

    printf("Here's what I got:\n%s\n", buf);

    const char *msg = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<html><body><h1>Hi there!</h1></body></html>\n";

    n = write(sock, msg, strlen(msg));
    if (n < 0) {
        perror("Oops, can't write to socket");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int mainSock, newSock, portNum, addrSize;
    struct sockaddr_in serverAddr, clientAddr;
    int clients[MAX_USERS] = {0};
    fd_set reads;
    int maxSock, currSock;

    if (argc < 2) {
        fprintf(stderr, "Need a port number, buddy\n");
        exit(1);
    }

    mainSock = socket(AF_INET, SOCK_STREAM, 0);
    if (mainSock < 0) {
        oopsie("Oops, can't open socket");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    portNum = atoi(argv[1]);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(portNum);

    if (bind(mainSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        oopsie("Oops, can't bind");
    }

    if (listen(mainSock, 5) < 0) {
        oopsie("Oops, can't listen");
    }
    printf("Server's up on port %d\n", portNum);

    addrSize = sizeof(clientAddr);

    while (1) {
        FD_ZERO(&reads);
        FD_SET(mainSock, &reads);
        maxSock = mainSock;

        for (int i = 0; i < MAX_USERS; i++) {
            currSock = clients[i];
            if (currSock > 0) {
                FD_SET(currSock, &reads);
            }
            if (currSock > maxSock) {
                maxSock = currSock;
            }
        }

        if (select(maxSock + 1, &reads, NULL, NULL, NULL) < 0 && errno != EINTR) {
            printf("Select says 'nope': %s\n", strerror(errno));
        }

        if (FD_ISSET(mainSock, &reads)) {
            newSock = accept(mainSock, (struct sockaddr *)&clientAddr, (socklen_t *)&addrSize);
            if (newSock < 0) {
                oopsie("Oops, can't accept connection");
            }
            printf("Got a visitor from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            for (int i = 0; i < MAX_USERS; i++) {
                if (clients[i] == 0) {
                    clients[i] = newSock;
                    printf("Added to slot %d\n", i);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_USERS; i++) {
            currSock = clients[i];
            if (FD_ISSET(currSock, &reads)) {
                if (processRequest(currSock) == -1) {
                    getpeername(currSock, (struct sockaddr*)&clientAddr, (socklen_t*)&addrSize);
                    printf("Bye bye %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                    close(currSock);
                    clients[i] = 0;
                }
            }
        }
    }

    close(mainSock);
    return 0;
}

