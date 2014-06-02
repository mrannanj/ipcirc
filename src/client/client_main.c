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
#include <signal.h>

#include "common/common.h"
#include "common/epoll_cont.h"

void handle_sigchld(int sig) {
  die2("child died");
}

void child(const char* host, int to[2], int from[2]) {
  const char cmd[] = "iirc-attach";
  if (dup2(to[0], STDIN_FILENO) < 0) die("dup2");
  if (dup2(from[1], STDOUT_FILENO) < 0) die("dup2");
  if (host) {
    if (execlp("ssh", "ssh", host, cmd, (char*)NULL) < 0) die("execlp");
  } else {
    if (execlp(cmd, cmd, (char*)NULL) < 0) die("execlp");
  }
}

int child_conn_add(struct epoll_cont* e, int to[2], int from[2]) {
  int slot = epoll_cont_add(e, from[0], to[1]);
  if (slot < 0) die2("no connection slot for child pipe");
  e->conns[slot].cbs[EV_AFTER_READ] = conn_write_to_slot;
  return slot;
}

int stdin_conn_add(struct epoll_cont* e) {
  int slot = epoll_cont_add(e, STDIN_FILENO, STDOUT_FILENO);
  if (slot < 0) die2("no connection slot for stdin");
  e->conns[slot].cbs[EV_AFTER_READ] = conn_write_to_slot;
  return slot;
}

void parent(int to[2], int from[2]) {
  struct epoll_cont e;
  epoll_cont_init(&e);
  int s1 = stdin_conn_add(&e);
  int s2 = child_conn_add(&e, to, from);
  e.conns[s1].data.u32 = s2;
  e.conns[s2].data.u32 = s1;
  epoll_cont_serve(&e);
  epoll_cont_destroy(&e);
}

int main(int argc, char** argv) {
  int to[2];
  int from[2];
  if (pipe(to) < 0) die("pipe");
  if (pipe(from) < 0) die("pipe");

  signal(SIGCHLD, handle_sigchld);

  pid_t pid = fork();
  if (pid < 0) die("fork");
  if (pid == 0) {
    child(argv[1], to, from);
  } else {
    parent(to, from);
  }
}
