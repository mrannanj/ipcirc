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
#include <assert.h>

#include "common/conn.h"
#include "common/unix_conn.h"
#include "common/common.h"
#include "common/epoll_cont.h"

int add_unix_conn(struct epoll_cont* e) {
  char path[UNIX_PATH_MAX];
  size_t len;
  if (!find_server_addr(path, &len))
    die2("backend is not running");

  int us = socket(AF_UNIX, SOCK_STREAM, 0);
  if (us < 0) die("socket");
  struct sockaddr_un sa = { .sun_family = AF_UNIX };
  memcpy(sa.sun_path, path, len);
  sa.sun_path[0] = '\0';

  int on = 1;
  if (setsockopt(us, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0)
    die("setsockopt");
  if (connect(us, (struct sockaddr*)&sa, sizeof(sa.sun_family) + len) < 0)
    die("connect");
  if (!unix_conn_verify_cred(us))
    die2("server has invalid credentials");

  int slot = unix_conn_init(e, us);
  if (slot < 0) die2("creating unix connection failed");
  struct conn* c = &e->conns[slot];
  c->cbs[EV_CLOSE] = conn_close_fatal;
  c->cbs[EV_AFTER_READ] = conn_write_to_slot;
  c->cbs[EV_READ] = conn_read;
  c->cbs[EV_WRITE] = conn_write;
  return slot;
}

int add_stdio(struct epoll_cont* e) {
  int slot = epoll_cont_add(e, STDIN_FILENO, STDOUT_FILENO);
  if (slot < 0) die2("failed to add stdin");
  struct conn* c = &e->conns[slot];
  c->cbs[EV_AFTER_READ] = conn_write_to_slot;
  c->cbs[EV_CLOSE] = conn_close_fatal;
  return slot;
}

int main(void) {
  struct epoll_cont e;
  epoll_cont_init(&e);
  int s1 = add_unix_conn(&e);
  int s2 = add_stdio(&e);
  e.conns[s1].data.u32 = s2;
  e.conns[s2].data.u32 = s1;
  epoll_cont_serve(&e);
  epoll_cont_destroy(&e);
}
