#pragma once

#include <stdint.h>

struct epoll_cont;
struct event;

int unix_listen_init();
int unix_listen_accept(int);
int unix_listen_read(struct epoll_cont*, uint32_t, struct event*);
