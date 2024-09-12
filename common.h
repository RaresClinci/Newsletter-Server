#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(errno);                                                             \
    }                                                                          \
  } while (0)


#define MAX_CONNECTIONS 32
#define MAX_EVENTS 10
#define TOPIC_MAXSIZE 50
#define BUFFER_SIZE 100
#define CONTENT_MAXSIZE 1500
#define MAX_MESSAGE_SIZE strlen("unsubscribe ") + TOPIC_MAXSIZE + 1
#define IP_ADDRESS_SIZE 16

// client request packet
enum Request : int {
  SUB, UNSUB, EXIT
};

struct Req_Package {
  int command;
  char topic[TOPIC_MAXSIZE + 1];
};

// UDP packet format
enum DataType : uint8_t {
  INT,
  SHORT_REAL,
  FLOAT,
  STRING
};

struct UDP_Packet {
  char topic[TOPIC_MAXSIZE];
  DataType type;
  char data[CONTENT_MAXSIZE];
};

struct ExtendedUDP {
  UDP_Packet core;
  uint16_t port;
  char ip_address[IP_ADDRESS_SIZE];
};

// TCP packet format
enum TCPPurpose : int {
  DISPLAY,
  COMMAND,
  ID,
  QUIT
};

struct TCPHeader {
  int purpose;
  unsigned int len;
};

// packet functions
int send_tcp(int fd, void* data, int len, TCPPurpose type);
void* recv_tcp(int fd);

#endif