#pragma once

#include <stdint.h>
#include <stddef.h>

struct event;
struct epoll_cont;
struct conn;

struct conn *unix_conn_init(struct epoll_cont *, int);

int unix_conn_irc_msg(struct epoll_cont *, struct conn *, struct event *);
int unix_conn_after_read(struct epoll_cont *, struct conn *, struct event *);
int unix_conn_verify_cred(int);
