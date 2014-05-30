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

#include "common/common.h"
#include "irc_conn.h"
#include "unix_listen.h"
#include "unix_conn.h"
#include "conn.h"

#include "epoll_cont.h"

void add_unix_listen(struct epoll_cont* e) {
  struct epoll_event ee;
  struct conn* c = &e->conns[e->nconn];
  c->fd = unix_listen_init();
  c->cbs[EV_READY_TO_READ] = unix_listen_read;
  ee.events = EPOLLIN;
  ee.data.u32 = e->nconn++;
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->fd, &ee) < 0) die("epoll_ctl");
}

void add_irc_conn(struct epoll_cont* e) {
  struct epoll_event ee;
  struct conn* c = &e->conns[e->nconn];
  irc_conn_init(c, "irc.freenode.net", 6667);
  ee.events = EPOLLIN;
  ee.data.u32 = e->nconn++;
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->fd, &ee) < 0) die("epoll_ctl");
}

int main(void) {
  srand(time(NULL));
  signal(SIGPIPE, SIG_IGN);
  struct epoll_cont e;
  epoll_cont_init(&e);
  add_unix_listen(&e);
  add_irc_conn(&e);
  epoll_cont_serve(&e);
  epoll_cont_destroy(&e);
}
