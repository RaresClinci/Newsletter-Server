#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <netinet/tcp.h>
#include <algorithm>

#include "common.h"
#define INC_CONN 5

using namespace std;

// client struct
struct Client {
    int socket;
    unordered_map<string, bool> topics;
    vector<string> wildcards;
};

int max_connections = 3;

int udp_connect;
int tcp_connect;
unordered_map<string, Client*> client_list;
struct pollfd *fds;
int num_connect = 3;

// build new client
Client* new_client(int sock, string id) {
    Client *c = new Client;
    c->socket = sock;

    // adding the client to the list
    client_list[id] = c;

    return c;
}

// handle new client
Client* handle_client(int sock, string id) {
    auto it = client_list.find(id);
    if (it != client_list.end()) {
        // client is in the map
        Client* c = it->second;
        if(c->socket == -1) {
            // client logged on before
            c->socket = sock;
            return c;
        } else {
            // client already logged
            return NULL;
        }
    } else {
        return new_client(sock, id);
    }
}

// initializing server
struct sockaddr_in init_server(uint16_t port) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    return servaddr;

}

// locate the client with this fd
string locate_client_by_fd(int fd) {
    for (auto& pair : client_list) {
        Client* client = pair.second;

        if (client->socket == fd) {
            return pair.first;
        }
    }

    return "";
}

// remove connection from polling system
void remove_connection(int fd) {
    int i, j;

    // locating the connection
    for(i = 0; i < num_connect; i++) {
        if(fd == fds[i].fd)
            break;
    }

    // removing it from the array
    for(j = i; j < num_connect - 1; j++) {
        fds[j] = fds[j + 1];
    }

    num_connect--;
}

// handle tcp command
int handle_tcp_command(string id, Req_Package packet) {
    Client* client = client_list[id];
    if (ntohl(packet.command) == SUB) {
        // user wants to sub to a topic
        string topic(packet.topic);

        if (strchr(packet.topic, '*') != NULL || strchr(packet.topic, '+') != NULL) {
            // topic is a wildcard
            client->wildcards.emplace_back(topic);
        } else {
            // topic is normal
            client->topics[topic] = true;
        }
        return 1;
    } else if (ntohl(packet.command) == UNSUB) {
        // user wants to unsub from a topic
        string topic(packet.topic);

        if (strchr(packet.topic, '*') != NULL || strchr(packet.topic, '+')) {
            // topic is a wildcard
            client->wildcards.erase(remove(client->wildcards.begin(), client->wildcards.end(), topic), client->wildcards.end());
        } else {
            // topic is normal
            client->topics.erase(topic);
        }
        return 1;
    } else if (ntohl(packet.command) == EXIT) {
        // closing the client(the client wants to close, no use sending QUIT packet)
        cout << "Client " << id << " disconnected." << endl;

        remove_connection(client->socket);
        close(client->socket);
        client->socket = -1;
    
        return 0;
    }
    return 1;
}

bool match_pattern(char* wildcard, char *topic) {
    char *saveptr1, *saveptr2;
    char* w = strtok_r(wildcard, "/", &saveptr1);
    char* t = strtok_r(topic, "/", &saveptr2);

    while(t && w) {
        if(strcmp(w, "*") != 0 && strcmp(w, "+") != 0 && strcmp(w, t) != 0) {
            // the words don't match
            return false;
        }

        // handling the * wildcard
        if(strcmp(w, "*") == 0) {
            w = strtok_r(NULL, "/", &saveptr1);
            if(w == NULL) {
                // anything matches
                return true;
            }

            // looking for end of *
            while(t != NULL && strcmp(t, w) != 0) {
                t = strtok_r(NULL, "/", &saveptr2);
            }

            if (t == NULL) {
                return false;
            }
        }

        t = strtok_r(NULL, "/", &saveptr2);
        w = strtok_r(NULL, "/", &saveptr1);
    }

    if(t == NULL && w == NULL) {
        // the words are equal
        return true;
    }

    return false;
}

bool match_wildcard(Client* c, char* topic) {
    for(string wild : c->wildcards) {
        // copying wildcard
        char* w = (char*)malloc(wild.length());
        std::strcpy(w, wild.c_str());

        // copying topic
        char* t = strdup(topic);

        if(match_pattern(w, t)) {
            free(w);
            free(t);
            return true;
        }

        free(w);
        free(t);
    }

    return false;
}

// forward packet function
void forward_packet(ExtendedUDP packet) {
    // adding '\0' to the end of the topic
    char topic[TOPIC_MAXSIZE + 1];
    memcpy(topic, packet.core.topic, TOPIC_MAXSIZE);
    topic[TOPIC_MAXSIZE] = 0;

    for(auto& pair : client_list) {
        Client* client = pair.second;

        // sending if topic is subscribed directly
        if(client->topics.find(topic) != client->topics.end() && client->topics[topic] == true) {
            if(client->socket != -1) {
                int ret = send_tcp(client->socket, &packet, sizeof(ExtendedUDP), DISPLAY);
                DIE(ret < 0, "send_tcp");
                return;
            }
        }

        // sending if the topic matches a wildcard
        if(match_wildcard(client, topic)) {
            int ret = send_tcp(client->socket, &packet, sizeof(ExtendedUDP), DISPLAY);
            DIE(ret < 0, "send_tcp");
        }
    }

    
}

// run server function
int run_server() {
    while(1) {
        int new_conn = 0;
        // looking for events
        int num_events = poll(fds, num_connect, -1);
        DIE(num_events < 0, "epoll_wait");

        // handling the events
        for(int i = 0; i < num_connect; i++) {
            if (fds[i].revents & POLLIN) {
                if(fds[i].fd == STDIN_FILENO) {
                    // we got the exit command?
                    string input;
                    getline(cin, input);
                    if(input == "exit") {
                        return 1;
                    } else {
                        // shouldn't be getting other stdin input => ignore
                        continue;
                    }
                } else if(fds[i].fd == tcp_connect) {
                    // new tcp connection
                    struct sockaddr_in client_addr;
                    socklen_t client_addrlen = sizeof(client_addr);
                    int tcp_fd = accept(tcp_connect, (struct sockaddr*)&client_addr, &client_addrlen);
                    DIE(tcp_fd < 0, "accept");

                    // reading the client id
                    char* buffer;
                    buffer = (char*)recv_tcp(tcp_fd);
                    DIE(buffer == NULL, "recv_tcp");

                    Client *c = handle_client(tcp_fd, buffer);

                    // client is already connected
                    if(c == NULL) {
                        printf("Client %s already connected.\n",
                            buffer);
                        
                        // sending client a QUIT message
                        int ret = send_tcp(tcp_fd, NULL, 0, QUIT);
                        DIE(ret < 0, "send_tcp");

                        close(tcp_fd);
                        continue;
                    }

                    // printing the connection message
                    printf("New client %s connected from %hu.\n",
                            buffer, ntohs(client_addr.sin_port));

                    // stopping the Neagle algorithm
                    int enable = 1;
                    int rc = setsockopt(tcp_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
                    DIE(rc < 0, "setsockopt(TCP_NODELAY) failed");

                    // adding the socket to epoll
                    if(num_connect + 1 > max_connections) {
                        // reallocing connection vector
                        max_connections += INC_CONN;
                        fds = (struct pollfd*)realloc(fds, (max_connections) * sizeof(struct pollfd));
                    }
                    
                    fds[num_connect].fd = tcp_fd;
                    fds[num_connect].events = POLLIN; 
                    new_conn = 1;
                } else if(fds[i].fd == udp_connect) {
                    // new upd message to forward
                    UDP_Packet packet;
                    memset(&packet, 0, sizeof(UDP_Packet));

                    struct sockaddr_in udp_client;
                    socklen_t len = sizeof(udp_client);

                    int ret = recvfrom(fds[i].fd, &packet, sizeof(UDP_Packet), 0,
                                (struct sockaddr *)&udp_client, &len);
                    DIE(ret < 0, "recvfrom");

                    // adding ip and port to the packet
                    ExtendedUDP final_packet;
                    memcpy(&(final_packet.core), &packet, sizeof(UDP_Packet));
                    strcpy(final_packet.ip_address, inet_ntoa(udp_client.sin_addr));
                    final_packet.port = ntohs(udp_client.sin_port);

                    forward_packet(final_packet);
                } else {
                    // command from client
                    Req_Package* packet = (Req_Package*)malloc(sizeof(Req_Package));
                    DIE(packet == NULL, "malloc");

                    memset(packet, 0, sizeof(Req_Package));

                    packet = (Req_Package*)recv_tcp(fds[i].fd);
                    DIE(packet == NULL, "recv_tcp");

                    string id = locate_client_by_fd(fds[i].fd);

                    int ret = handle_tcp_command(id, *packet);
                    if(!ret) {
                        // the fds vector was reorganised, decrementing i
                        i--;
                    }
                    free(packet);
                }
            }
        }

        // a new client connected, incrementing the number of connections
        if(new_conn == 1) {
            num_connect++;
        }
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // allocing the connection vector
    fds = (struct pollfd*)malloc(max_connections * sizeof(struct pollfd));

    // adding stdin to epoll
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // creating server
    uint16_t port = 0;
    DIE(argc != 2, "number of arguments");
    sscanf(argv[1], "%hu", &port);

    struct sockaddr_in servaddr = init_server(port);

    // defining the UDP socket
    udp_connect = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_connect < 0, "socket");

    // enabling socket reuse
    int enable = 1;
    int rc = setsockopt(udp_connect, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEPORT) failed");

    // binding the udp socket
    rc = bind(udp_connect, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(rc < 0, "bind udp");

    // adding udp to epoll
    fds[1].fd = udp_connect;
    fds[1].events = POLLIN;

    // defining the TCP socket
    tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_connect < 0, "socket");

    // enabling socket reuse
    enable = 1;
    rc = setsockopt(tcp_connect, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEPORT) failed");

    // binding the tcp socket
    rc = bind(tcp_connect, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(rc < 0, "bind tcp");

    // listening on tcp
    rc = listen(tcp_connect, MAX_CONNECTIONS - 2);

    // adding udp to epoll
    fds[2].fd = tcp_connect;
    fds[2].events = POLLIN;

    // running the server
    run_server();

    // closing sockets
    for(int i = 3; i < num_connect; i++) {
        rc = send_tcp(fds[i].fd, NULL, 0, QUIT);
        DIE(rc < 0, "send_tcp");
        close(fds[i].fd);
    }
    
    return 0;
}