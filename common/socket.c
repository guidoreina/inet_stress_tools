#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include "common/socket.h"
#include "common/macros.h"

int build_socket_address(const char* str,
                         struct sockaddr_storage* addr,
                         socklen_t* addrlen)
{
  struct sockaddr_un* sun;
  const char* colon;
  const char* p;
  char host[64];
  unsigned port;
  size_t len;

  /* Unix socket? */
  if (*str == '/') {
    sun = (struct sockaddr_un*) addr;

    if ((len = strlen(str)) >= sizeof(sun->sun_path)) {
      return -1;
    }

    sun->sun_family = AF_UNIX;

    memcpy(sun->sun_path, str, len);
    sun->sun_path[len] = 0;

    *addrlen = sizeof(struct sockaddr_un);

    return 0;
  }

  /* Search the last colon (for IPv6 there might be more than one). */
  colon = NULL;
  p = str;
  while (*p) {
    if (*p == ':') {
      colon = p;
    }

    p++;
  }

  /* If there is no port... */
  if (!colon) {
    return -1;
  }

  if (((len = colon - str) == 0) || (len >= sizeof(host))) {
    return -1;
  }

  p = colon + 1;
  port = 0;
  while (*p) {
    if (!IS_DIGIT(*p)) {
      return -1;
    }

    if ((port = (port * 10) + (*p - '0')) > 65535) {
      return -1;
    }

    p++;
  }

  if (port == 0) {
    return -1;
  }

  memcpy(host, str, len);
  host[len] = 0;

  return build_ip_address(host, port, addr, addrlen);
}

int build_ip_address(const char* str,
                     in_port_t port,
                     struct sockaddr_storage* addr,
                     socklen_t* addrlen)
{
  unsigned char buf[sizeof(struct in6_addr)];
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;

  /* Try first with IPv4. */
  if (inet_pton(AF_INET, str, buf) <= 0) {
    if (inet_pton(AF_INET6, str, buf) <= 0) {
      return -1;
    }

    sin6 = (struct sockaddr_in6*) addr;

    memset(sin6, 0, sizeof(struct sockaddr_in6));

    sin6->sin6_family = AF_INET6;
    memcpy(&sin6->sin6_addr, buf, sizeof(struct in6_addr));
    sin6->sin6_port = htons(port);

    *addrlen = sizeof(struct sockaddr_in6);
  } else {
    sin = (struct sockaddr_in*) addr;

    sin->sin_family = AF_INET;
    memcpy(&sin->sin_addr, buf, sizeof(struct in_addr));
    sin->sin_port = htons(port);
    memset(sin->sin_zero, 0, sizeof(sin->sin_zero));

    *addrlen = sizeof(struct sockaddr_in);
  }

  return 0;
}

int socket_connect(const struct sockaddr* addr, socklen_t addrlen)
{
  int fd;

  /* Create socket. */
  if ((fd = socket_create(addr->sa_family, SOCK_STREAM)) < 0) {
    return -1;
  }

  /* Connect. */
  do {
    if (connect(fd, addr, addrlen) < 0) {
      switch (errno) {
        case EINTR:
          continue;
        case EINPROGRESS:
          return fd;
        default:
          close(fd);
          return -1;
      }
    } else {
      return fd;
    }
  } while (1);
}

int socket_get_error(int fd, int* error)
{
  socklen_t errorlen = sizeof(int);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, error, &errorlen) < 0) {
    return -1;
  }

  return 0;
}

int socket_listen(const struct sockaddr* addr, socklen_t addrlen)
{
  int fd;

  /* Create socket. */
  if ((fd = socket_create(addr->sa_family, SOCK_STREAM)) < 0) {
    return -1;
  }

  /* Bind. */
  if (socket_bind(fd, addr, addrlen) < 0) {
    close(fd);
    return -1;
  }

  /* Listen. */
  if (listen(fd, SOMAXCONN) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

int socket_accept(int fd, struct sockaddr* addr, socklen_t* addrlen)
{
  int client;

  do {
    if ((client = accept4(fd, addr, addrlen, SOCK_NONBLOCK)) < 0) {
      if (errno != EINTR) {
        return -1;
      }
    } else {
      return client;
    }
  } while (1);
}

ssize_t socket_send(int fd, const void* buf, size_t len)
{
  ssize_t ret;
  while (((ret = send(fd, buf, len, MSG_NOSIGNAL)) < 0) && (errno == EINTR));
  return ret;
}

ssize_t socket_recv(int fd, void* buf, size_t len)
{
  ssize_t ret;
  while (((ret = recv(fd, buf, len, 0)) < 0) && (errno == EINTR));
  return ret;
}

int socket_bind(int fd, const struct sockaddr* addr, socklen_t addrlen)
{
  /* Reuse address. */
  int optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
    return -1;
  }

#ifdef SO_REUSEPORT
  /* Reuse port. */
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int)) < 0) {
    return -1;
  }
#endif /* SO_REUSEPORT */

  return bind(fd, addr, addrlen);
}

int socket_make_non_blocking(int fd)
{
#if USE_FIONBIO
  int value;

  value = 1;

  if (ioctl(fd, FIONBIO, &value) < 0) {
    return -1;
  }
#else
  int flags;

  flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0) {
    return -1;
  }
#endif

  return 0;
}

int socket_create(int domain, int type)
{
  int fd;

  if ((fd = socket(domain, type, 0)) < 0) {
    return -1;
  }

  if (socket_make_non_blocking(fd) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}
