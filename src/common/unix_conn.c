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

#include "common/epoll_cont.h"
#include "common/event.h"
#include "common/unix_conn.h"
#include "common/common.h"

static void check_for_messages(struct epoll_cont*, uint32_t, ssize_t);

static const size_t MSG_MAX = 512;

void unix_conn_init(struct epoll_cont* e, int fd) {
  int slot = epoll_cont_find_free(e);
  if (slot < 0) {
    log("connection slots used, not accepting unix connection %d", fd);
    close(fd);
    return;
  }

  struct conn* nc = &e->conns[slot];
  nc->fd = fd;
  nc->cbs[EV_READY_TO_READ] = unix_conn_read;
  nc->cbs[EV_IRC_MESSAGE] = unix_conn_irc_msg;
  nc->cbs[EV_CLOSE] = unix_conn_close;
  nc->data.unix.pos = 0;
  nc->data.unix.buf = malloc(MSG_MAX+1);
  struct epoll_event ee = { .events = EPOLLIN, .data.u32 = slot };
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, nc->fd, &ee) < 0) die("epoll_ctl");
  struct event ev = { .type = EV_UNIX_ACCEPTED, .source = slot };
  epoll_cont_walk(e, &ev);
}

int unix_conn_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  unix_conn* uc = &c->data.unix;
  ssize_t nread = read(c->fd, &uc->buf[uc->pos], MSG_MAX-uc->pos);
  if (nread == 0) {
    log("EOF from unix_conn %d", c->fd);
    return 0;
  } else if (nread < 0) {
    log_errno("read");
    return 0;
  }
  check_for_messages(e, p, nread);
  return 1;
}

int unix_conn_irc_msg(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  ssize_t len = strlen(ev->p);
  if (strncmp(ev->p, "PONG", 4) == 0) return 1;
  ssize_t nwrote = write(c->fd, ev->p, len);
  if (nwrote < 0) {
    log_errno("write");
    return 0;
  } else if (len != nwrote) {
    log("partial write on unix_conn %d", c->fd);
    return 0;
  }
  return 1;
}

int unix_conn_close(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  if (c->fd != -1) {
    if (epoll_ctl(e->epfd, EPOLL_CTL_DEL, c->fd, NULL) < 0)
      log_errno("epoll_ctl");
    if (close(c->fd) < 0)
      log_errno("close");
  }
  if (c->data.unix.buf) free(c->data.unix.buf);
  memset(c, 0, sizeof(*c));
  c->fd = -1;
  return 1;
}

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
