#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <netinet/tcp.h>

#include "common.h"

using namespace std;

// parsing ip address
uint32_t ip_to_int(const std::string& ip) {
    struct in_addr ip_addr;
    int ret = inet_pton(AF_INET, ip.c_str(), &ip_addr);
    DIE(ret != 1, "Invalid ip address");
    return ntohl(ip_addr.s_addr);
}

Req_Package* parse_command(char *msg) {
    Req_Package* packet = (Req_Package*)malloc(sizeof(Req_Package));
    memset(packet, 0, sizeof(Req_Package));

    // the command is invalid if it's empty
    if(strlen(msg) == 0) {
        return NULL;
    }

    // checking if there is more than one space
    if(strchr(msg, ' ') != strrchr(msg, ' ')) {
        return NULL;
    }

    char *p = strtok(msg, " ");

    if(p == NULL) {
        return NULL;
    }

    // turning command into packet
    if(strcmp(p, "exit") == 0) {
        packet->command = htonl(EXIT);
    } else if(strcmp(p, "subscribe") == 0) {
        packet->command = htonl(SUB);
        p = strtok(NULL, " ");
        strcpy(packet->topic, p);
        printf("Subscribed to topic %s\n", p);
    } else if(strcmp(p, "unsubscribe") == 0) {
        packet->command = htonl(UNSUB);
        p = strtok(NULL, " ");
        strcpy(packet->topic, p);
        printf("Unsubscribed from topic %s\n", p);
    } else {
        return NULL;
    }

    return packet;
}

void display_udp(ExtendedUDP* packet) {
    cout << packet->ip_address << ':' << packet->port << " - ";
    cout << packet->core.topic << " - ";

    // printing the data
    char* data = packet->core.data;
    if(packet->core.type == INT) {
        cout << "INT - ";
        // sign byte, printing minus if sign isn't 0
        uint8_t sign = *(uint8_t*) data;

        // the number
        uint32_t num = ntohl(*(uint32_t*)(data + sizeof(uint8_t)));

        if(sign && num != 0)
            cout << "-";

        printf("%u\n", num);
    } else if (packet->core.type == SHORT_REAL) {
        cout << "SHORT_REAL - ";
        uint16_t num = ntohs(*(uint16_t*) data);
        printf("%.2f\n", num / 100.0);
    } else if (packet->core.type == FLOAT) {
        cout << "FLOAT - ";
        // sign byte, printing minus if sign isn't 0
        uint8_t sign = *(uint8_t*) data;
        if(sign)
            cout << "-";

        // the number
        uint32_t num = ntohl(*(uint32_t*)(data + sizeof(uint8_t)));

        // the power
        uint8_t power = *(uint8_t*)(data + sizeof(uint8_t) + sizeof(uint32_t));

        // creating the true number
        double x = num * 1.0;
        for(int i = 0; i < power; i++) {
            x /= 10.0;
        }

        printf("%0.4f\n", x);
    } else if (packet->core.type == STRING) {
        cout << "STRING - ";
        char msg[CONTENT_MAXSIZE + 1];
        msg[CONTENT_MAXSIZE] = 0;

        memcpy(msg, data, CONTENT_MAXSIZE);

        cout << msg << "\n";
    }
}

void run_client(int sockfd) {
    // multiplexing the inputs
    struct pollfd fds[2];

    // adding the socket and the stdin
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    while(1) {
        int ret = poll(fds, 2, -1);
        DIE(ret < 0, "poll");

        if(fds[0].revents & POLLIN) {
            // event on stdin
            char msg[MAX_MESSAGE_SIZE];

            fgets(msg, MAX_MESSAGE_SIZE, stdin);
            msg[strlen(msg) - 1] = 0;

            Req_Package* packet = parse_command(msg);
            if(packet == NULL) {
                continue;
            }

            ret = send_tcp(sockfd, packet, sizeof(Req_Package), COMMAND);
            DIE(ret < 0, "send_tcp");
            if(ntohl(packet->command) == EXIT) {
                return;
            }
        }

        if(fds[1].revents & POLLIN) {
            // event on socket
            ExtendedUDP* packet;

            packet = (ExtendedUDP*)recv_tcp(sockfd);
            if(packet == NULL) {
                return;
            }

            display_udp(packet);
            free(packet);
        }
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 4, "number of arguments");

    // parsing the id
    char *id = (char*)argv[1];

    // parsing the ip of the server
    uint32_t ip_address = ip_to_int(argv[2]);

    // parsing the port
    uint16_t port = 0;
    int ret = sscanf((char*)argv[3], "%hu", &port);
    DIE(ret != 1, "Invalid port");

    // creating tcp socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    // stopping the Neagle algorithm
    int enable = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    DIE(ret < 0, "setsockopt(TCP_NODELAY) failed");

    // connecting client to server
    ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(ret < 0, "connect");

    // sending the id
    ret = send_tcp(sockfd, id, strlen(id) + 1, ID);
    DIE(ret < 0, "send_tcp");

    run_client(sockfd);

    close(sockfd);
    return 0;
}