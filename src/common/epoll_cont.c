#include "common/epoll_cont.h"
#include "common/event.h"
#include "common/common.h"

#include <sys/epoll.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void epoll_close_conn(struct epoll_cont*, uint32_t);

void epoll_cont_init(struct epoll_cont* e) {
  memset(e, 0, sizeof(*e));
  e->cont = 1;
  e->epfd = epoll_create1(EPOLL_CLOEXEC);
  if (e->epfd < 0) die("epoll_create1");
  for (int i = 0; i < MAX_CONN; ++i) e->conns[i].rfd = -1;
}

void epoll_cont_serve(struct epoll_cont* e) {
  struct epoll_event es[MAX_EVENTS];

  while (e->cont) {
    int nfd = epoll_wait(e->epfd, es, MAX_EVENTS, -1);
    if (nfd < 0) die("epoll_wait");
    for (int i = 0; i < nfd; ++i) {
      uint32_t p = es[i].data.u32;
      if (es[i].events & EPOLLIN) {
        if (e->conns[p].rfd == -1) continue;
        if (!e->conns[p].cbs[EV_READ](e, p, NULL)) {
          epoll_close_conn(e, p);
        }
      }
      if (es[i].events & EPOLLOUT) {
        if (e->conns[p].wfd == -1) continue;
        if (!e->conns[p].cbs[EV_WRITE](e, p, NULL)) {
          epoll_close_conn(e, p);
        }
      }
    }
  }
}

void epoll_cont_walk(struct epoll_cont* e, struct event* ev) {
  for (size_t p = 0; p < MAX_CONN; ++p) {
    if (e->conns[p].rfd == -1) continue;
    event_cb cb = e->conns[p].cbs[ev->type];
    if (!cb) continue;
    if (!cb(e, p, ev)) epoll_close_conn(e, p);
  }
}

void epoll_cont_destroy(struct epoll_cont* e) {
  close(e->epfd);
}

void epoll_close_conn(struct epoll_cont* e, uint32_t p) {
  log("closing connection %d", p);
  assert(p < MAX_CONN);
  event_cb cb = e->conns[p].cbs[EV_CLOSE];
  if (cb) {
    e->cont = cb(e, p, NULL);
  } else {
    log("conn %d does not have cleanup cb");
  }
}

int epoll_cont_find_free(struct epoll_cont* e) {
  for (int p = 0; p < MAX_CONN; ++p)
    if (e->conns[p].rfd == -1)
      return p;
  return -1;
}

int epoll_cont_add(struct epoll_cont* e, int rfd, int wfd) {
  int slot = conn_init(e, rfd, wfd);
  if (slot < 0) return -1;
  struct conn* c = &e->conns[slot];
  c->rfd = rfd;
  c->wfd = wfd;
  c->cbs[EV_CLOSE] = conn_close;
  c->cbs[EV_READ] = conn_read;
  c->cbs[EV_WRITE] = conn_write;
  return slot;
}

