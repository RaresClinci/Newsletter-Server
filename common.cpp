#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>

#include "common.h"

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char*)buffer;

  while(bytes_remaining > 0) {
    ssize_t received = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    DIE(received < 0, "recv");

    if(received == 0) {
        // conection closed
        break;
    }

    bytes_received += received;
    bytes_remaining -= received;
  }

  return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char*)buffer;

  while(bytes_remaining) {
    ssize_t sent = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    DIE(sent < 0, "send");

    bytes_sent += sent;
    bytes_remaining -= sent;
  }
  return bytes_sent;
}

int send_tcp(int fd, void* data, int len, TCPPurpose type) {
    // creating and sending header
    TCPHeader header;
    header.purpose = htonl(type);
    header.len = htonl(len);
    int ret = send_all(fd, &header, sizeof(TCPHeader));
    DIE(ret < 0, "send_all");

    // sending the payload
    ret = send_all(fd, data, len);
    DIE(ret < 0, "send_all");

    return len;
}

void* recv_tcp(int fd) {
    // receiving the header
    TCPHeader header;
    int ret = recv_all(fd, &header, sizeof(TCPHeader));
    DIE(ret < 0, "recv_all");

    // what sort of massage do we receive now?
    int purpose = ntohl(header.purpose);
    int len = ntohl(header.len);
    if(purpose == DISPLAY) {
      // an udp packet was forwarder
      ExtendedUDP *packet = (ExtendedUDP*)malloc(len);
      DIE(packet == NULL, "malloc");

      recv_all(fd, packet, len);
      DIE(ret < 0, "recv_all");

      return packet;
    } else if(purpose == COMMAND) {
      // command packet
      Req_Package *packet = (Req_Package*)malloc(len);
      DIE(packet == NULL, "malloc");
      memset(packet, 0, sizeof(Req_Package));

      recv_all(fd, packet, len);

      return packet;
    } else if(purpose == ID) {
      // user id
      char* id = (char*)malloc(len);
      DIE(id == NULL, "malloc");

      recv_all(fd, id, len);

      return id;
    }

    // the packet is QUIT type
    return NULL;
}