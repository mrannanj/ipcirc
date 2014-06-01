#include "epoll_cont.h"
#include "event.h"
#include "common/common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>

static void epoll_close_conn(struct epoll_cont*, uint32_t);

void epoll_cont_init(struct epoll_cont* e) {
  e->epfd = epoll_create1(0);
  if (e->epfd < 0) die("epoll_create1");
  memset(e->conns, 0, sizeof(struct conn) * MAX_CONN);
  for (size_t i = 0; i < MAX_CONN; ++i) e->conns[i].fd = -1;
}

void epoll_cont_serve(struct epoll_cont* e) {
  struct epoll_event es[MAX_EVENTS];

  for (;;) {
    int nfd = epoll_wait(e->epfd, es, MAX_EVENTS, -1);
    if (nfd < 0) die("epoll_wait");
    for (int i = 0; i < nfd; ++i) {
      uint32_t p = es[i].data.u32;
      if (e->conns[p].fd == -1) continue;
      if (!e->conns[p].cbs[EV_READY_TO_READ](e, p, NULL)) {
        epoll_close_conn(e, p);
      }
    }
  }
}

void epoll_cont_walk(struct epoll_cont* e, struct event* ev) {
  for (size_t p = 0; p < MAX_CONN; ++p) {
    if (e->conns[p].fd == -1) continue;
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
    cb(e, p, NULL);
  } else {
    log("conn %d does not have cleanup cb");
  }
}

int epoll_cont_find_free(struct epoll_cont* e) {
  for (int p = 0; p < MAX_CONN; ++p)
    if (e->conns[p].fd == -1)
      return p;
  return -1;
}
