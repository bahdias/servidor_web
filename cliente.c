#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {

	struct sockaddr_in serveraddr;
    int client;

	if((client = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket() error!");
		exit(1);
	}

	memset(&(serveraddr), 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &serveraddr.sin_addr.s_addr);
	serveraddr.sin_port = htons(8087);
   
    if(connect(client, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
    	perror("socket() error!");
		exit(1);
    }    
    
    char get[] = "GET / HTTP/1.0\r\nHost: www.pudim.com.br\r\n\r\n";
    printf("%ld\n", strlen(get));

    write(client, get, strlen(get));

    char buf[1024];

    read(client, buf, 13);
    printf("%s\n", buf);

    close(client);
}
