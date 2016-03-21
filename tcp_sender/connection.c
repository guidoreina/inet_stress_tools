#include <stdlib.h>
#include <sys/resource.h>
#include "tcp_sender/connection.h"

int connections_create(const options_t* options, connections_t* connections)
{
  struct rlimit rlim;
  connection_t* conn;
  unsigned i;

  /* Get the maximum number of file descriptors. */
  if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
    connections->connections = NULL;
    connections->nconnections = 0;

    return -1;
  }

  if ((connections->connections =
       (connection_t*) malloc(rlim.rlim_cur * sizeof(connection_t))) == NULL) {
    connections->nconnections = 0;
    return -1;
  }

  for (i = 0; i < rlim.rlim_cur; i++) {
    conn = &connections->connections[i];

    if ((conn->recvbuf = (uint8_t*) malloc(options->receive)) == NULL) {
      for (; i > 0; i--) {
        free(connections->connections[i - 1].recvbuf);
      }

      free(connections->connections);
      connections->connections = NULL;

      connections->nconnections = 0;

      return -1;
    }

    conn->options = options;

    connection_clear(conn);
  }

  connections->nconnections = rlim.rlim_cur;

  return 0;
}

void connections_free(connections_t* connections)
{
  unsigned i;

  if (connections->connections) {
    for (i = 0; i < connections->nconnections; i++) {
      free(connections->connections[i].recvbuf);
    }

    free(connections->connections);
    connections->connections = NULL;
  }

  connections->nconnections = 0;
}

void connection_clear(connection_t* connection)
{
  connection->state = STATE_NOT_CONNECTED;
  connection->nloops = 0;
  connection->sent = 0;
  connection->received = 0;
  connection->nfile = 0;
  connection->index = -1;
  connection->readable = 0;
  connection->writable = 0;
}
