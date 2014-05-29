#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "common/common.h"

#define BUFSIZE 4096

char buf[BUFSIZE];

int main(void) {
  char path[UNIX_PATH_MAX];
  size_t len;
  if (!find_server_addr(path, &len)) {
    printf("server not running, should start it\n");
    exit(1);
  }

  int us = socket(AF_UNIX, SOCK_STREAM, 0);
  if (us < 0) die("socket");
  struct sockaddr_un sa = { .sun_family = AF_UNIX };
  memcpy(sa.sun_path, path, len);
  sa.sun_path[0] = '\0';
  if (connect(us, (struct sockaddr*)&sa, sizeof(sa.sun_family) + len) < 0)
    die("connect");

  int epfd = epoll_create1(0);
  struct epoll_event ee = { .events = EPOLLIN, .data.fd = us };
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, us, &ee) < 0) die("epoll_ctl");

  ee.events = EPOLLIN;
  ee.data.fd = STDIN_FILENO;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ee) < 0) die("epoll_ctl");

  for (;;) {
    int nfd = epoll_wait(epfd, &ee, 1, -1);
    if (nfd < 0) die("epoll_wait");
    ssize_t nread = read(ee.data.fd, buf, BUFSIZE);
    if (nread < 0) die("read");
    if (nread == 0) break;
    int to = ee.data.fd == STDIN_FILENO ? us : STDOUT_FILENO;
    ssize_t nwrote = write(to, buf, nread);
    if (nwrote < 0) die("write");
    if (nwrote != nread) die2("partial write, exiting");
  }
  close(us);
}
