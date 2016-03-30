#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "udp_sender/worker.h"

static void usage(const char* program);

int main(int argc, const char** argv)
{
  struct sockaddr_storage addr;
  socklen_t addrlen;
  options_t options;
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

  /* Create worker threads. */
  if (workers_create((const struct sockaddr*) &addr,
                     addrlen,
                     &options,
                     &running,
                     &ready,
                     &workers) < 0) {
    fprintf(stderr, "Error creating worker threads.\n");

    options_free(&options);
    return -1;
  }

  /* Join worker threads. */
  workers_join(&workers, &running);

  /* Show statistics. */
  workers_statistics(&workers);

  options_free(&options);

  return 0;
}

void usage(const char* program)
{
  printf("Usage: %s [OPTIONS] <address>\n", program);
  printf("\t<address>: socket address where to send messages to. "
         "Possible formats:\n");
  printf("\t\t<host>:<port>: IPv4/IPv6 address and port.\n");
  printf("\t\tUnix socket: Unix socket as absolute path.\n");
  printf("\n");
  printf("\tOptions:\n");
  printf("\t\t--messages-per-send <count> (range: %u - %u, default: %u).\n",
         MIN_MESSAGES_PER_SEND,
         MAX_MESSAGES_PER_SEND,
         DEFAULT_MESSAGES_PER_SEND);
  printf("\t\t--sends <count> (range: %llu - %llu, default: %llu).\n",
         MIN_SENDS,
         MAX_SENDS,
         DEFAULT_SENDS);
  printf("\t\t--delay <microseconds> (range: %u - %u, default: %u).\n",
         MIN_DELAY,
         MAX_DELAY,
         DEFAULT_DELAY);
  printf("\t\t--threads <count> (range: %u - %u, default: %u).\n",
         MIN_THREADS,
         MAX_THREADS,
         DEFAULT_THREADS);
  printf("\t\t--processors <processor-list> | \"even\" | \"odd\".\n");
  printf("\t\t\t<processor-list> ::= <processor>[,<processor>]*\n");
  printf("\t\t--file <filename>: file to be sent. --file <filename> can be "
         "used multiple times.\n");
  printf("\t\t\tIf not specified, a dummy file of %u byte%s is sent.\n",
         DUMMY_FILESIZE,
         (DUMMY_FILESIZE != 1) ? "s" : "");
  printf("\n");
}
