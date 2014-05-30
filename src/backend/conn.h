#pragma once

#include "event.h"
#include "irc_conn.h"
#include "unix_conn.h"

struct epoll_cont;

typedef int (*event_cb)(struct epoll_cont*, uint32_t, struct event*);

struct conn {
  int fd;
  event_cb cbs[EV_COUNT];
  union {
    irc_conn irc;
    unix_conn unix;
  } data;
};
