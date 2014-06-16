#pragma once

#include <stdint.h>

struct epoll_cont;
struct event;
struct conn;

int unix_listen_init();
int unix_listen_accept(int);
int unix_listen_read(struct epoll_cont *, struct conn *, struct event *);
