#include <iostream>
#include <vector>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "helpers.h"

#define BUF_LEN 2001

using namespace std;

struct protocol {
    int opcode;
    char topic[51];
    char ip[16];
    uint16_t port;
    char content[1500];
};

int parse_stdin(char *buffer, string &topic) {
    if (strncmp(buffer, "exit", 4) == 0) {
        return 0;
    }
    char *token = strtok(buffer, " ");
    if (strcmp(token, "subscribe") == 0) {
        token = strtok(NULL, " ");
        topic = token;
        return 1;
    }
    if (strcmp(token, "unsubscribe") == 0) {
        token = strtok(NULL, " ");
        topic = token;
        return 2;
    }
    return -1;
}

void case_stdin(int sockfd, fd_set &read_fds) {
    char buffer[BUF_LEN];
    memset(buffer, 0, BUF_LEN);
    fgets(buffer, BUF_LEN - 1, stdin);
    buffer[strlen(buffer) - 1] = '\0';
    string topic;
    int ret = parse_stdin(buffer, topic);
    if (ret == 0) {
        struct protocol message;
        bzero(&message, sizeof(message));
        message.opcode = 10;
        int rc = send(sockfd, (char *)&message, sizeof(message), 0);
        DIE(rc < 0, "send");
        close(sockfd);
        exit(0);
    }
    if (ret == 1) {
        struct protocol message;
        bzero(&message, sizeof(message));
        message.opcode = 11;
        strcpy(message.topic, topic.c_str());
        int rc = send(sockfd, (char *)&message, sizeof(message), 0);
        cout << "Subscribed to topic " << message.topic << endl;
        DIE(rc < 0, "send");
    }
    if (ret == 2) {
        struct protocol message;
        bzero(&message, sizeof(message));
        message.opcode = 12;
        strcpy(message.topic, topic.c_str());
        int rc = send(sockfd, (char *)&message, sizeof(message), 0);
        cout << "Unsubscribed from topic " << message.topic << endl;
        DIE(rc < 0, "send");
    }
}


int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
        exit(0);
    }
    int port = atoi(argv[3]);

    //get id_client
    char id_client[11];
    strcpy(id_client, argv[1]);

    //get ip_server
    char tmp_ip_server[16];
    strcpy(tmp_ip_server, argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    int rc = inet_aton(tmp_ip_server, &serv_addr.sin_addr);
    DIE(rc == 0, "inet_aton");

    //disable Nagle's algorithm
    int enable = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int));
    DIE(rc < 0, "setsockopt");

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    rc = send(sockfd, id_client, strlen(id_client), 0);
    DIE(rc < 0, "send");
    
    fd_set read_fds, tmp_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    for (;;) {
        tmp_fds = read_fds;
        rc = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(rc < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            case_stdin(sockfd, read_fds);
        }
        if (FD_ISSET(sockfd, &tmp_fds)) {
            // am primit mesaj de la server
            struct protocol message;
            memset(&message.content, 0, 1500);
            rc = recv(sockfd, (char *)&message, sizeof(message), 0);
            DIE(rc < 0, "recv");
            if (rc == 0) {
                break;
            }
            if (message.opcode == -1) {
                // exit
                close(sockfd);
                exit(0);
            }
            if (message.opcode == 0) {
                //am primit un int
                int value;
                char sign = message.content[0];
                memcpy(&value, message.content + 1, sizeof(int));
                value = ntohl(value);
                char udp_ip[16];
                bzero(udp_ip, 16);
                strcpy(udp_ip, message.ip);
                int udp_port = message.port;
                if (sign == 0) {
                    printf("%s:%d - %s - %s - %d\n", udp_ip, udp_port, message.topic, "INT", value);
                } else {
                    printf("%s:%d - %s - %s - -%d\n", udp_ip, udp_port, message.topic, "INT", value);
                }
                continue;
            }
            if (message.opcode == 1) {
                //am primit un short_real
                uint16_t number;
                number = ntohs(*(uint16_t *)message.content);
                float value = number / 100.0;
                char udp_ip[16];
                bzero(udp_ip, 16);
                strcpy(udp_ip, message.ip);
                int udp_port = message.port;
                printf("%s:%d - %s - %s - %.2f\n", udp_ip, udp_port, message.topic, "SHORT_REAL", value);
                continue;
            }
            if (message.opcode == 2) {
                //am primit un float
                uint32_t number;
                char sign = message.content[0];
                memcpy(&number, message.content + 1, sizeof(uint32_t));
                number = ntohl(number);
                uint8_t power;
                memcpy(&power, message.content + 5, sizeof(uint8_t));
                float value = number / pow(10, power);
                char udp_ip[16];
                bzero(udp_ip, 16);
                strcpy(udp_ip, message.ip);
                int udp_port = message.port;
                if (sign == 0) {
                    printf("%s:%d - %s - %s - %.*f\n", udp_ip, udp_port, message.topic, "FLOAT", power, value);
                } else {
                    printf("%s:%d - %s - %s - -%.*f\n", udp_ip, udp_port, message.topic, "FLOAT", power, value);
                }
                continue;
            }
            if (message.opcode == 3) {
                //am primit un string
                char udp_ip[16];
                bzero(udp_ip, 16);
                strcpy(udp_ip, message.ip);
                int udp_port = message.port;
                message.content[strlen(message.content)] = '\0';
                printf("%s:%d - %s - %s - %s\n", udp_ip, udp_port, message.topic, "STRING", message.content);
                continue;
            }
        }
    }

    return 0;
}