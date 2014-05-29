#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#include <assert.h>

#include "epoll_cont.h"
#include "unix_conn.h"
#include "common/common.h"

static const size_t LISTEN_BACKLOG = 10;
static const size_t MSG_MAX = 512;

void check_for_messages(struct epoll_cont* e, uint32_t p, ssize_t nread) {
  unix_conn* uc = &e->conns[p].data.unix;
  char msg[IRC_MAXLEN];
  for (ssize_t k = 0; k < nread;) {
    char c = uc->buf[uc->pos+k];
    if (c == '\n') {
      memcpy(msg, uc->buf, uc->pos+k+1);
      msg[uc->pos+k+1] = '\0';
      struct event ev = { .type = EV_UNIX_MESSAGE, .source = p, .p = msg };
      epoll_cont_walk(e, &ev);
      memmove(uc->buf, &uc->buf[uc->pos+k+1], nread - (k+1));
      uc->pos = 0;
      nread -= k+1;
      k = 0;
    } else {
      ++k;
    }
  }
}

void unix_conn_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  unix_conn* uc = &c->data.unix;
  ssize_t nread = read(c->fd, &uc->buf[uc->pos], MSG_MAX-uc->pos);
  if (nread == 0) die2("unix connection lost\n");
  if (nread < 0) die("read");
  check_for_messages(e, p, nread);
}

void unix_conn_irc_msg(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  ssize_t len = strlen(ev->p);
  ssize_t nwrote = write(c->fd, ev->p, len);
  if (nwrote < 0) die("write");
  if (len != nwrote) die2("partial write");
}

void unix_listen_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  assert(e->nconn < MAX_CONN);
  struct conn* c = &e->conns[p];
  struct conn* nc = &e->conns[e->nconn];
  nc->fd = unix_listen_accept(c->fd);
  nc->cbs[EV_READY_TO_READ] = unix_conn_read;
  nc->cbs[EV_IRC_MESSAGE] = unix_conn_irc_msg;
  nc->data.unix.pos = 0;
  nc->data.unix.buf = malloc(MSG_MAX+1);
  struct epoll_event ee = { .events = EPOLLIN, .data.u32 = e->nconn++ };
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, nc->fd, &ee) < 0) die("epoll_ctl");
}

int unix_listen_init() {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) die("socket");
  size_t path_len = strlen(UNIX_ADDR_PREFIX);
  struct sockaddr_un sa = { .sun_family = AF_UNIX };
  memcpy(sa.sun_path, UNIX_ADDR_PREFIX, path_len);
  sa.sun_path[0] = '\0';
  if (bind(fd, (struct sockaddr*)&sa, sizeof(sa.sun_family) + path_len) < 0)
    die("bind");
  if (listen(fd, LISTEN_BACKLOG) < 0) die("listen");
  return fd;
}

int unix_listen_accept(int s) {
  struct sockaddr_un client;
  socklen_t addrlen = sizeof(client);
  int acc = accept(s, (struct sockaddr*)&client, &addrlen);
  if (acc < 0) die("accept");
  return acc;
}
