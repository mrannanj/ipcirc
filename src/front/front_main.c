#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/epoll.h>

#include "common/common.h"

void usage(char* inv) {
  printf("usage: %s <hostname>\n", inv);
  exit(0);
}

const char remote_cmd[] = "./ipc-proxy";

void child(int to[2], int from[2], const char* host) {
  if (dup2(to[0], STDIN_FILENO) < 0) die("dup2");
  if (dup2(from[1], STDOUT_FILENO) < 0) die("dup2");
  const char cmd[] = "ssh";
  if (execlp(cmd, cmd, host, remote_cmd, (char*)NULL) < 0) die("execlp");
}

#define MAX_EVENTS 10
#define BUFSIZE 1024

void parent(int to[2], int from[2]) {
  (void)to;
  int epfd = epoll_create1(0);
  if (epfd < 0) die("epoll_create1");
  struct epoll_event event = { .events = EPOLLIN, .data.fd = from[0] };
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, from[0], &event) < 0) die("epoll_ctl");
  event.events = EPOLLIN;
  event.data.fd = STDIN_FILENO;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0)
    die("epoll_ctl");

  struct epoll_event es[MAX_EVENTS];
  char buf[BUFSIZE];

  while (1) {
    int nfd = epoll_wait(epfd, es, MAX_EVENTS, -1);
    if (nfd < 0) die("epoll_wait");
    for (int i = 0; i < nfd; ++i) {
      int efd = es[i].data.fd;
      ssize_t nread = read(efd, buf, BUFSIZE);
      if (nread < 0) die("read");
      if (nread == 0) die2("fd dead, quittin");
      int tofd = (efd == from[0] ? STDOUT_FILENO : to[1]);
      ssize_t nwrote = write(tofd, buf, nread);
      if (nwrote < 0) die("write");
      if (nwrote != nread) die2("partial write");
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) usage(argv[0]);
  int to[2];
  if (pipe(to) < 0) die("pipe");
  int from[2];
  if (pipe(from) < 0) die("pipe");

  signal(SIGCHLD, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  pid_t pid = fork();
  if (pid < 0) die("fork");
  if (pid == 0) {
    child(to, from, argv[1]);
  } else {
    parent(to, from);
  }
}
