#pragma once

#include "common/event.h"
#include "common/unix_conn.h"
#include "backend/irc_conn.h"

#define CONN_BUFSIZ 65536

struct epoll_cont;

typedef int (*event_cb)(struct epoll_cont*, uint32_t, struct event*);

struct conn {
  int fd;
  event_cb cbs[EV_COUNT];
  int in_pos;
  char* in_buf;
  char* out_buf;
  union {
    void* ptr;
    uint32_t u32;
    uint64_t u64;
  } data;
};
