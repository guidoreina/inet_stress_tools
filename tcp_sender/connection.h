#ifndef CONNECTION_H
#define CONNECTION_H

#include "tcp_sender/options.h"

typedef enum {
  STATE_NOT_CONNECTED,
  STATE_CONNECTING,
  STATE_SENDING,
  STATE_RECEIVING
} connection_state_t;

typedef struct {
  const options_t* options;

  uint8_t* recvbuf;

  connection_state_t state;
  uint64_t nloops;
  unsigned sent;
  unsigned received;
  size_t nfile;

  int index;

  int readable;
  int writable;
} connection_t;

typedef struct {
  connection_t* connections;
  unsigned nconnections;
} connections_t;

int connections_create(const options_t* options, connections_t* connections);
void connections_free(connections_t* connections);

void connection_clear(connection_t* connection);

#endif /* CONNECTION_H */
