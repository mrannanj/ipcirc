#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
  size_t pos;
  char* buf;
} unix_conn;

struct event;
struct epoll_cont;

void unix_conn_read(struct epoll_cont*, uint32_t, struct event*);
void unix_conn_irc_msg(struct epoll_cont*, uint32_t, struct event*);

int unix_listen_init();
int unix_listen_accept(int);
void unix_listen_read(struct epoll_cont*, uint32_t, struct event*);
