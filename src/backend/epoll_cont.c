#include "epoll_cont.h"
#include "event.h"
#include "common/common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>

void epoll_cont_init(struct epoll_cont* e) {
  e->epfd = epoll_create1(0);
  e->nconn = 0;
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
      if (!e->conns[p].cbs[EV_READY_TO_READ](e, p, NULL))
        e->conns[p].cbs[EV_CLOSE](e, p, NULL);
    }
  }
}

void epoll_cont_walk(struct epoll_cont* e, struct event* ev) {
  for (size_t i = 0; i < MAX_CONN; ++i) {
    if (e->conns[i].fd == -1) continue;
    event_cb cb = e->conns[i].cbs[ev->type];
    if (!cb) continue;
    cb(e, i, ev);
  }
}

void epoll_cont_destroy(struct epoll_cont* e) {
  close(e->epfd);
}
