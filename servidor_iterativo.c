#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // Cria um socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erro ao criar o socket do servidor");
        exit(1);
    }

    // Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Liga o socket ao endereço do servidor
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao vincular o socket ao endereço do servidor");
        exit(1);
    }

    // Escuta por conexões
    if (listen(server_socket, 10) == 0) {
        printf("Servidor aguardando conexões...\n");
    } else {
        perror("Erro na escuta");
        exit(1);
    }

    while (1) {
        // Aceita a conexão do cliente
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Erro ao aceitar a conexão do cliente");
            continue;
        }

        printf("Conexão aceita do endereço: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Lógica de processamento para este cliente
        char request[1024];
        int bytes_received = recv(client_socket, request, sizeof(request), 0);
        if (bytes_received < 0) {
            perror("Erro ao receber dados do cliente");
        } else {
            // Analise a requisição HTTP aqui
            request[bytes_received] = '\0'; // Garanta que a string seja terminada corretamente

            // Exemplo de análise da requisição
            char method[10], url[100], version[20], host[100];
            if (sscanf(request, "%s %s %s\r\nHost: %s", method, url, version, host) == 4) {
                printf("Método HTTP: %s\n", method);
                printf("URL: %s\n", url);
                printf("Versão HTTP: %s\n", version);
                printf("Host: %s\n", host);
            } else {
                printf("Requisição inválida.\n");
            }
        }

        // Fecha o socket do cliente
        close(client_socket);
    };

    // Fecha o socket do servidor
    close(server_socket);

    return 0;
}