#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "tcp_receiver/worker.h"

static void usage(const char* program);

int main(int argc, const char** argv)
{
  struct sockaddr_storage addr;
  socklen_t addrlen;
  options_t options;
  connections_t connections;
  workers_t workers;
  uint8_t running, ready;
  sigset_t set;

  /* Check arguments. */
  if (argc == 1) {
    usage(argv[0]);
    return -1;
  }

  if (build_socket_address(argv[argc - 1], &addr, &addrlen) < 0) {
    fprintf(stderr, "Invalid socket address '%s'.\n", argv[argc - 1]);
    return -1;
  }

  if (options_parse(argc, argv, &options) < 0) {
    usage(argv[0]);
    return -1;
  }

  /* Block signals SIGINT and SIGTERM. */
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  if (pthread_sigmask(SIG_BLOCK, &set, NULL) < 0) {
    fprintf(stderr, "Error blocking signals.\n");

    options_free(&options);
    return -1;
  }

  /* Create connections. */
  if (connections_create(&options, &connections) < 0) {
    fprintf(stderr, "Error creating connections.\n");

    options_free(&options);
    return -1;
  }

  /* Create worker threads. */
  if (workers_create((const struct sockaddr*) &addr,
                     addrlen,
                     &options,
                     &connections,
                     &running,
                     &ready,
                     &workers) < 0) {
    fprintf(stderr, "Error creating worker threads.\n");

    connections_free(&connections);
    options_free(&options);

    return -1;
  }

  /* Join worker threads. */
  workers_join(&workers, &options, &connections, &running);

  /* Show statistics. */
  workers_statistics(&workers);

  connections_free(&connections);
  options_free(&options);

  return 0;
}

void usage(const char* program)
{
  printf("Usage: %s [OPTIONS] <address>\n", program);
  printf("\t<address>: socket address where to bind to. "
         "Possible formats:\n");
  printf("\t\t<host>:<port>: IPv4/IPv6 address and port.\n");
  printf("\t\tUnix socket: Unix socket as absolute path.\n");
  printf("\n");
  printf("\tOptions:\n");
  printf("\t\t--threads <count> (range: %u - %u, default: %u).\n",
         MIN_THREADS,
         MAX_THREADS,
         DEFAULT_THREADS);
  printf("\t\t--receive <count>: number of bytes to receive (range: %u - %u, "
         "default: %u).\n",
         MIN_RECEIVE,
         MAX_RECEIVE,
         DEFAULT_RECEIVE);
  printf("\t\t--connection-loops <count>:\n");
  printf("\t\t\tNumber of iterations send/receive per connection\n");
  printf("\t\t\t(range: %llu - %llu, default: %llu).\n",
         MIN_LOOPS,
         MAX_LOOPS,
         DEFAULT_LOOPS);
  printf("\t\t--client-sends-first | --server-sends-first:\n");
  printf("\t\t\tWho sends data first? (default: %s).\n",
         CLIENT_SENDS_FIRST ? "--client-sends-first" : "--server-sends-first");
  printf("\t\t--set-read-write-event | --do-not-set-read-write-event:\n");
  printf("\t\t\tShould both events be set or only one? (default: %s).\n",
         SET_READ_WRITE_EVENT ? "--set-read-write-event" :
                                "--do-not-set-read-write-event");

#ifdef SO_REUSEPORT
  printf("\t\t--single-listener | --one-listener-per-thread:\n");
  printf("\t\t\tSame listener for all the threads or a listener per thread?\n");
  printf("\t\t\t(default: %s).\n",
         SINGLE_LISTENER ? "--single-listener" : "--one-listener-per-thread");
#endif /* SO_REUSEPORT */

  printf("\t\t--single-epoll-fd | --one-epoll-fd-per-thread:\n");
  printf("\t\t\tSame epoll fd for all the threads or a epoll fd per thread?\n");
  printf("\t\t\t(default: %s).\n",
         SINGLE_EPOLL_FD ? "--single-epoll-fd" : "--one-epoll-fd-per-thread");
  printf("\t\t--processors <processor-list> | \"even\" | \"odd\".\n");
  printf("\t\t\t<processor-list> ::= <processor>[,<processor>]*\n");
  printf("\t\t--file <filename>: file to be sent. --file <filename> can be "
         "used multiple times.\n");
  printf("\n");
}
