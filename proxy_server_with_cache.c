#include "proxy_parse.h"
#include <stdio.h>

struct ParsedRequest;  
typedef struct ParsedRequest ParsedRequest;
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400
#define MAX_SIZE 200 * (1 << 20)
#define MAX_ELEMENT_SIZE 10 * (1 << 20)

typedef struct cache_element {
    char *data;
    int len;
    char *url;
    time_t lru_time_track;
    struct cache_element *next;
} cache_element;

cache_element *find(char *url);
int add_cache_element(char *data, int size, char *url);
void remove_cache_element();

int port_number = 8080;
int proxy_socketId;
pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;

cache_element *head;
int cache_size;

int sendErrorMessage(int socket, int status_code) {
    char str[1024];
    char currentTime[50];
    time_t now = time(0);
    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code) {
        case 400:
            snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\n</BODY></HTML>", currentTime);
            break;
        case 403:
            snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
            break;
        case 404:
            snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
            break;
        case 500:
            snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
            break;
        case 501:
            snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
            break;
        case 505:
            snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nAryanS/01021\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
            break;
        default:
            return -1;
    }
    send(socket, str, strlen(str), 0);
    return 1;
}

int connectRemoteServer(char *host_addr, int port_num) {
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0) {
        perror("Error in Creating Socket.\n");
        return -1;
    }

    struct hostent *host = gethostbyname(host_addr);
    if (!host) {
        fprintf(stderr, "No such host exists.\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    bcopy((char *)host->h_addr_list[0], (char *)&server_addr.sin_addr.s_addr, host->h_length);

    if (connect(remoteSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in connecting !\n");
        return -1;
    }
    return remoteSocket;
}

int handle_request(int clientSocket, ParsedRequest *request, char *tempReq) {
    char *buf = (char *)malloc(MAX_BYTES);
    snprintf(buf, MAX_BYTES, "GET %s %s\r\n", request->path, request->version);

    size_t len = strlen(buf);

    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        perror("Set header key not working\n");
    }

    if (!ParsedHeader_get(request, "Host")) {
        if (ParsedHeader_set(request, "Host", request->host) < 0) {
            perror("Set \"Host\" header key not working\n");
        }
    }

    if (ParsedRequest_unparse_headers(request, buf + len, MAX_BYTES - len) < 0) {
        perror("Unparse failed\n");
    }

    int server_port = request->port ? atoi(request->port) : 80;
    int remoteSocketID = connectRemoteServer(request->host, server_port);
    if (remoteSocketID < 0) {
        free(buf);
        return -1;
    }

    send(remoteSocketID, buf, strlen(buf), 0);
    bzero(buf, MAX_BYTES);

    int bytes_recv = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    char *temp_buffer = (char *)malloc(MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while (bytes_recv > 0) {
        send(clientSocket, buf, bytes_recv, 0);
        for (int i = 0; i < bytes_recv; i++) {
            temp_buffer[temp_buffer_index++] = buf[i];
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char *)realloc(temp_buffer, temp_buffer_size);
        bzero(buf, MAX_BYTES);
        bytes_recv = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    }
    temp_buffer[temp_buffer_index] = '\0';
    free(buf);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    free(temp_buffer);
    close(remoteSocketID);
    return 0;
}

int checkHTTPversion(char *msg) {
    if (strncmp(msg, "HTTP/1.1", 8) == 0 || strncmp(msg, "HTTP/1.0", 8) == 0) {
        return 1;
    }
    return -1;
}

void *thread_fn(void *socketNew) {
    sem_wait(&semaphore);
    int socket = *(int *)socketNew;
    int bytes_recv_client, len;

    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    bytes_recv_client = recv(socket, buffer, MAX_BYTES, 0);

    while (bytes_recv_client > 0) {
        len = strlen(buffer);
        if (!strstr(buffer, "\r\n\r\n")) {
            bytes_recv_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        } else {
            break;
        }
    }

    char *tempReq = strdup(buffer);
    cache_element *temp = find(tempReq);

    if (temp) {
        int size = temp->len;
        int pos = 0;
        char response[MAX_BYTES];
        while (pos < size) {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES && pos < size; i++) {
                response[i] = temp->data[pos++];
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data retrieved from the Cache\n\n");
    } else if (bytes_recv_client > 0) {
        len = strlen(buffer);
        ParsedRequest *request = ParsedRequest_create();
        if (ParsedRequest_parse(request, buffer, len) < 0) {
            perror("Parsing failed\n");
        } else {
            bzero(buffer, MAX_BYTES);
            if (!strcmp(request->method, "GET")) {
                if (request->host && request->path && checkHTTPversion(request->version) == 1) {
                    if (handle_request(socket, request, tempReq) == -1) {
                        sendErrorMessage(socket, 500);
                    }
                } else {
                    sendErrorMessage(socket, 500);
                }
            } else {
                printf("This code doesn't support any method other than GET\n");
            }
        }
        ParsedRequest_destroy(request);
    } else if (bytes_recv_client < 0) {
        perror("Error in receiving from client.\n");
    } else if (bytes_recv_client == 0) {
        printf("Client disconnected!\n");
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    free(tempReq);
    return NULL;
}

int main(int argc, char *argv[]) {
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if (argc == 2) {
        port_number = atoi(argv[1]);
    } else {
        fprintf(stderr, "Too few arguments\n");
        exit(1);
    }

    printf("Setting Proxy Server Port : %d\n", port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0) {
        perror("Failed to create socket.\n");
        exit(1);
    }

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed\n");
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Port is not free\n");
        exit(1);
    }
    printf("Binding on port: %d\n", port_number);

    if (listen(proxy_socketId, MAX_CLIENTS) < 0) {
        perror("Error while Listening !\n");
        exit(1);
    }

    int i = 0;
    int Connected_socketId[MAX_CLIENTS];

    while (1) {
        bzero(&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if (client_socketId < 0) {
            perror("Error in Accepting connection !\n");
            exit(1);
        } else {
            Connected_socketId[i] = client_socketId;
        }

        struct sockaddr_in *client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number: %d and ip address: %s \n", ntohs(client_addr.sin_port), str);

        pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketId[i]);
        i++;
    }
    close(proxy_socketId);
    return 0;
}

cache_element *find(char *url) {
    cache_element *site = NULL;
    pthread_mutex_lock(&lock);
    if (head) {
        site = head;
        while (site) {
            if (!strcmp(site->url, url)) {
                site->lru_time_track = time(NULL);
                break;
            }
            site = site->next;
        }
    }
    pthread_mutex_unlock(&lock);
    return site;
}

void remove_cache_element() {
    pthread_mutex_lock(&lock);
    if (head) {
        cache_element *p = head, *q = head, *temp = head;
        while (q->next) {
            if (q->next->lru_time_track < temp->lru_time_track) {
                temp = q->next;
                p = q;
            }
            q = q->next;
        }
        if (temp == head) {
            head = head->next;
        } else {
            p->next = temp->next;
        }
        cache_size -= temp->len + sizeof(cache_element) + strlen(temp->url) + 1;
        free(temp->data);
        free(temp->url);
        free(temp);
    }
    pthread_mutex_unlock(&lock);
}

int add_cache_element(char *data, int size, char *url) {
    pthread_mutex_lock(&lock);

    int element_size = size + 1 + strlen(url) + sizeof(cache_element);
    if (element_size > MAX_ELEMENT_SIZE) {
        pthread_mutex_unlock(&lock);
        return 0;
    }

    while (cache_size + element_size > MAX_SIZE) {
        remove_cache_element();
    }

    cache_element *element = (cache_element *)malloc(sizeof(cache_element));
    if (!element) {
        perror("Failed to allocate memory for cache element");
        pthread_mutex_unlock(&lock);
        return 0;
    }

    element->data = (char *)malloc(size + 1);
    if (!element->data) {
        perror("Failed to allocate memory for cache data");
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    strcpy(element->data, data);

    element->url = (char *)malloc(strlen(url) + 1);
    if (!element->url) {
        perror("Failed to allocate memory for cache URL");
        free(element->data);
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    strcpy(element->url, url);

    element->lru_time_track = time(NULL);
    element->next = head;
    element->len = size;
    head = element;
    cache_size += element_size;

    pthread_mutex_unlock(&lock);
    return 1;
}