#pragma once

#include <stdint.h>

struct event;
struct epoll_cont;
struct conn;

int attach_conn_close(struct epoll_cont *, struct conn *, struct event *);
int attach_conn_after_read(struct epoll_cont *, struct conn *, struct event *);

struct conn *attach_conn_add(struct epoll_cont *, const char *);
void attach_conn_parent(int[2], int[2]);
void attach_conn_exec(const char *, int[2], int[2]);
