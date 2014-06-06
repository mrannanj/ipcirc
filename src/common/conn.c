#include "common/conn.h"
#include "common/common.h"
#include "common/epoll_cont.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

int conn_init(struct epoll_cont* e, int rfd, int wfd) {
  int slot = epoll_cont_find_free(e);
  if (slot < 0) return -1;
  struct conn* c = &e->conns[slot];
  memset(c, 0, sizeof(*c));

  c->in_pos = 0;
  c->in_buf = malloc(CONN_BUFSIZ);
  assert(c->in_buf);

  c->out_pos = 0;
  c->out_buf = malloc(CONN_BUFSIZ);
  assert(c->out_buf);

  c->rfd = rfd;
  struct epoll_event ee = { .events = EPOLLIN, .data.u32 = slot };
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->rfd, &ee) < 0)
    die("epoll_ctl");

  c->wfd = wfd;
  ee.events = 0;
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->wfd, &ee) < 0)
    die("epoll_ctl");

  return slot;
}

int conn_close_fatal(struct epoll_cont* e, uint32_t slot, struct event* ev) {
  conn_close(e,slot,ev);
  return 0;
}

int conn_close(struct epoll_cont* e, uint32_t slot, struct event* ev) {
  struct conn* c = &e->conns[slot];
  if (c->rfd != -1) {
    if (epoll_ctl(e->epfd, EPOLL_CTL_DEL, c->rfd, NULL) < 0)
      log_errno("epoll_ctl");
    if (close(c->rfd) < 0)
      log_errno("close");
  }
  if (c->wfd != -1) {
    if (epoll_ctl(e->epfd, EPOLL_CTL_DEL, c->wfd, NULL) < 0)
      log_errno("epoll_ctl");
    if (close(c->wfd) < 0)
      log_errno("close");
  }
  if (c->in_buf) free(c->in_buf);
  if (c->out_buf) free(c->out_buf);
  memset(c, 0, sizeof(*c));
  c->rfd = c->wfd = -1;
  return 1;
}

int conn_read(struct epoll_cont* e, uint32_t slot, struct event* ev) {
  struct conn* c = &e->conns[slot];
  ssize_t nread = read(c->rfd, &c->in_buf[c->in_pos], CONN_BUFSIZ - c->in_pos);
  if (nread < 0) {
    log_errno("read");
    return 0;
  } else if (nread == 0) {
    log("EOF from conn %d, fd %d", slot, c->rfd);
    return 0;
  }
  c->in_pos += nread;
  c->cbs[EV_AFTER_READ](e, slot, NULL);
  return 1;
}

int conn_write(struct epoll_cont* e, uint32_t slot, struct event* ev) {
  struct conn* c = &e->conns[slot];
  ssize_t nwrote = write(c->wfd, c->out_buf, c->out_pos);
  if (nwrote < 0) {
    log_errno("write");
    return 0;
  }
  c->out_pos -= nwrote;
  if (c->out_pos == 0) {
    struct epoll_event ee = { .events = 0, .data.u32 = slot };
    epoll_ctl(e->epfd, EPOLL_CTL_MOD, c->wfd, &ee);
  } else {
    memmove(c->out_buf, &c->out_buf[nwrote], c->out_pos);
  }
  return 1;
}

int conn_write_to_slot(struct epoll_cont* e, uint32_t s, struct event* ev) {
  struct conn* source = &e->conns[s];
  int r = conn_write_buf2(e, source->data.u32, source->in_buf, source->in_pos);
  source->in_pos = 0;
  return r;
}

int conn_write_buf2(struct epoll_cont* e, uint32_t s, char* b, ssize_t len) {
  struct conn* c = &e->conns[s];
  if (len > CONN_BUFSIZ - c->out_pos) {
    log("connection %d, fd %d write buffer full", s, c->wfd);
    return 0;
  }
  memcpy(&c->out_buf[c->out_pos], b, len);
  c->out_pos += len;
  struct epoll_event ee = { .events = EPOLLOUT, .data.u32 = s };
  if (epoll_ctl(e->epfd, EPOLL_CTL_MOD, c->wfd, &ee) < 0) {
    log_errno("epoll_ctl");
  }
  return 1;
}
