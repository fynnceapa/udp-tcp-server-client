#include <iostream>
#include <vector>
#include <map>

#include <bits/stdc++.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "helpers.h"

#define BUF_LEN 1551

using namespace std;

struct protocol {
    int opcode;
    char topic[51];
    char ip[16];
    uint16_t port;
    char content[1500];
};

struct udp_message {
    // din enunt
    char topic[50];
    char type;
    char content[1500];
};

struct client {
    int socket;
    char id[11];
    char ip[16];
    uint16_t port;
    bool is_online;
    map<string, int> substriptions;
};

bool check_client (vector<client> &clients, char *id, int socket, sockaddr_in client_addr) {
    for (client &c : clients) {
        if (strcmp(c.id, id) == 0 && c.is_online) {
            int rc = send(socket, "exit", 4, 0);
            DIE(rc < 0, "send");
            close(socket);
            printf("Client %s already connected.\n", id);
            return false;
        }
        if (strcmp(c.id, id) == 0 && !c.is_online) {
            c.socket = socket;
            c.is_online = true;
            printf("New client %s connected from %s:%d.\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            return false;
        }
    }
    return true;
}

void add_client(vector<client> &clients, char *id, int socket, sockaddr_in client_addr) {
    client new_client;
    new_client.socket = socket;
    strcpy(new_client.id, id);
    new_client.is_online = true;
    new_client.port = ntohs(client_addr.sin_port);
    inet_ntop(AF_INET, &client_addr.sin_addr, new_client.ip, 16);
    clients.push_back(new_client);
    printf("New client %s connected from %s:%d.\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

protocol generate_message(int opcode, char *topic, char *content, char *ip, uint16_t port) {
    struct protocol p;
    memset(&p, 0, sizeof(p));
    memset(p.content, 0, 1500);
    p.opcode = opcode;
    strcpy(p.topic, topic);
    memcpy(p.content, content, 1500);
    strcpy(p.ip, ip);
    p.port = port;
    return p;
}

int send_message(client &client, protocol message) {
    if (!client.is_online) {
        return 0;
    }
    if (client.substriptions.find(message.topic) == client.substriptions.end()) {
        return 0;
    }
    int rc = send(client.socket, (char *)&message, sizeof(message), 0);
    DIE(rc < 0, "send");
    return 1;
}

int main(int argc, char *argv[]) {
    // dezactivez buffering-ul pentru stdout
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 2) {
        printf("please provide a port number\n");
        return 1;
    }
    in_port_t port = atoi(argv[1]);
    
    // creez socketul udp
    int udp_socket = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket < 0, "socket");

    // creez structura pentru udp
    struct sockaddr_in udp_addr;
    socklen_t udp_addr_len = sizeof(udp_addr);
    memset(&udp_addr, 0, sizeof(udp_addr));

    // creez structura pentru server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // fac bind la socketul udp
    int rc = bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind");

    // creez structura pentru client
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_addr_len = sizeof(client_addr);

    // creez socketul tcp
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket < 0, "socket");

    // fac bind la socketul tcp
    rc = bind(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind");

    // fac listen la socketul tcp pentru a putea accepta conexiuni
    if (listen(tcp_socket, 1) != 0) {
        perror("listen");
        return 1;
    }

    int nagle = 1;
	rc = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
	DIE(rc < 0, "Nagle");

    //setez optiunea SO_REUSEADDR pentru a putea folosi acelasi port
    int flag = 1;
    rc = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    DIE(rc < 0, "setsockopt");

    //multiplexare
    fd_set read_fds;
    vector<client> clients;
    int max_fd = max(tcp_socket, udp_socket);
    for (;;) {
        FD_ZERO(&read_fds);
        FD_SET(tcp_socket, &read_fds);
        FD_SET(udp_socket, &read_fds);
        FD_SET(0, &read_fds);

        for (client &c : clients) {
            if (c.socket != 0) {
                FD_SET(c.socket, &read_fds);
            }
            if (c.socket > max_fd) {
                max_fd = c.socket;
            }
        }
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        DIE(activity < 0, "select");

        if (FD_ISSET(0, &read_fds)) {
            char buffer[BUF_LEN];
            memset(buffer, 0, BUF_LEN);
            fgets(buffer, BUF_LEN - 1, stdin);
            if (strncmp(buffer, "exit", 4) == 0) {
                for (client &c : clients) {
                    if (c.is_online) {
                        struct protocol message;
                        memset(&message.content, 0, 1500);
                        message.opcode = -1;
                        int rc = send(c.socket, (char *)&message, sizeof(message), 0);
                        DIE(rc < 0, "send");
                    }
                }
                close(tcp_socket);
                close(udp_socket);
                break;
            }
        }
        if (FD_ISSET(tcp_socket, &read_fds)) {
            int new_socket = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            DIE(new_socket < 0, "accept");
            char buffer[BUF_LEN];
            memset(buffer, 0, BUF_LEN);
            rc = recv(new_socket, buffer, 11, 0);
            DIE(rc < 0, "recv");
            if (check_client(clients, buffer, new_socket, client_addr)) {
                add_client(clients, buffer, new_socket, client_addr);
            }
        }

        if (FD_ISSET(udp_socket, &read_fds)) {
            // am primit un mesaj pe udp
            char buffer[sizeof(struct udp_message)];
            memset(buffer, 0, sizeof(struct udp_message));
            rc = recvfrom(udp_socket, buffer, sizeof(struct udp_message), 0, (struct sockaddr *)&client_addr, &client_addr_len);
            struct udp_message *udp = (struct udp_message *)buffer;
            DIE(rc < 0, "recvfrom");
            char ip[16];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, 16);
            int port = ntohs(client_addr.sin_port);
            int type = udp->type;
            protocol message = generate_message(type, udp->topic, udp->content, ip, port);
            for (client &c : clients) {
                send_message(c, message);
            }
            continue;
        }
        for (int i = 0; i < clients.size(); i++) {
            if (FD_ISSET(clients[i].socket, &read_fds)) {
                char buffer[BUF_LEN];
                bzero(buffer, BUF_LEN);
                int rc = recv(clients[i].socket, buffer, BUF_LEN, 0);
                DIE(rc < 0, "recv");
                struct protocol *msg = (struct protocol *)buffer;
                if (msg->opcode == 10) {
                    //nu sterg clientul din vector si nici nu sterg topicurile la care este abonat
                    char ip[16];
                    inet_ntop(AF_INET, &server_addr.sin_addr, ip, 16);
                    int port = ntohs(server_addr.sin_port);
                    printf("Client %s disconnected.\n", clients[i].id);
                    clients[i].socket = 0;
                    clients[i].is_online = false;
                    continue;
                }
                if (msg->opcode == 11) {
                    if (clients[i].substriptions.find(msg->topic) == clients[i].substriptions.end()) {
                        clients[i].substriptions[msg->topic] = 1;
                    }
                    continue;
                }
                if (msg->opcode == 12) {
                    if (clients[i].substriptions.find(msg->topic) != clients[i].substriptions.end()) {
                        clients[i].substriptions.erase(msg->topic);
                    }
                    continue;
                }
            }
        }
    }
    return 0;
}