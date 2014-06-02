#pragma once

#include <stdint.h>
#include <stddef.h>

struct event;
struct epoll_cont;

int unix_conn_init(struct epoll_cont*, int);
int unix_conn_irc_msg(struct epoll_cont*, uint32_t, struct event*);
int unix_conn_after_read(struct epoll_cont*, uint32_t, struct event*);
