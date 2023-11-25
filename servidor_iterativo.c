#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define FILE_PATH "image.jpg"

void send_response(int client_socket, int status, const char *status_text, const char *content_type, const char *content, long content_length) {
    char response[1024];

    // Construa a resposta HTTP com cabeçalhos adicionais
    sprintf(response, "HTTP/1.1 %d %s\r\nAccept-Ranges: bytes\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n%s", status, status_text, content_length, content_type, content);

    // Envie os cabeçalhos para o cliente
    send(client_socket, response, strlen(response), 0);

    // Envie o conteúdo do arquivo para o cliente
    send(client_socket, content, content_length, 0);
}

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
            char method[10], url[100], version[20];
            if (sscanf(request, "%s %s %s\r\n", method, url, version) == 3) {
                printf("Método HTTP: %s\n", method);
                printf("URL: %s\n", url);
                printf("Versão HTTP: %s\n", version);

                // Verifique se a URL solicitada é a esperada
                if (strcmp(url, "/image.jpg") == 0) {
                    FILE *file = fopen(FILE_PATH, "rb");
                    if (file) {
                        fseek(file, 0, SEEK_END);
                        long file_size = ftell(file);
                        fseek(file, 0, SEEK_SET);

                        char *file_content = (char *)malloc(file_size);
                        if (!file_content) {
                            perror("Erro ao alocar memória para o conteúdo do arquivo");
                            fclose(file);
                            send_response(client_socket, 500, "Internal Server Error", "text/html", "500 Internal Server Error", 0);
                            continue;
                        }

                        fread(file_content, 1, file_size, file);
                        fclose(file);

                        // Envie uma resposta de sucesso (200) com o conteúdo do arquivo
                        send_response(client_socket, 200, "OK", "image/jpeg", file_content, file_size);

                        free(file_content);
                    } else {
                        // Envie uma resposta de erro (404) se o arquivo não puder ser aberto
                        perror("Erro ao abrir o arquivo");
                        send_response(client_socket, 404, "Not Found", "text/html", "404 Not Found", 0);
                    }
                } else {
                    // Envie uma resposta de erro (404) se a URL solicitada não for reconhecida
                    send_response(client_socket, 404, "Not Found", "text/html", "404 Not Found", 0);
                }
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