#pragma once

#include "event.h"
#include "irc_conn.h"
#include "unix_conn.h"

#define CONN_IRC 1
#define CONN_UNIX 2
#define CONN_UNIX_ACC 3

struct epoll_cont;

typedef void (*event_cb)(struct epoll_cont*, uint32_t, struct event*);

struct conn {
  int fd;
  event_cb cbs[EV_COUNT];
  union {
    irc_conn irc;
    unix_conn unix;
  } data;
};
