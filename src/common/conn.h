#pragma once

#include <unistd.h>

#include "common/event.h"
#include "common/unix_conn.h"
#include "common/tcp_conn.h"

#define CONN_BUFSIZ 65536
#define IRC_MAXLEN 513

#define EV_CLOSE 0
#define EV_READ 1
#define EV_AFTER_READ 2
#define EV_WRITE 3
#define EV_STDIN_IN 4

#define EV1_UNIX_ACCEPTED 5
#define EV1_IRC_MESSAGE 6
#define EV1_UNIX_MESSAGE 7

#define EV_MAX 10

struct epoll_cont;

typedef int (*event_cb)(struct epoll_cont*, uint32_t, struct event*);

struct conn {
  int rfd;
  int wfd;
  event_cb cbs[EV_MAX];
  ssize_t in_pos;
  ssize_t out_pos;
  char* in_buf;
  char* out_buf;
  union {
    void* ptr;
    uint32_t u32;
    uint64_t u64;
  } data;
};

int conn_init(struct epoll_cont*, int, int);
int conn_close(struct epoll_cont*, uint32_t, struct event*);
int conn_read(struct epoll_cont*, uint32_t, struct event*);
int conn_write(struct epoll_cont*, uint32_t, struct event*);
int conn_write_to_slot(struct epoll_cont*, uint32_t, struct event*);
int conn_write_buf2(struct epoll_cont*, uint32_t, char*, ssize_t);