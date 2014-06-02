#pragma once

#include "common/conn.h"

#include <stddef.h>
#include <stdint.h>

#define IRC_NLINES 20

struct irc_data {
  int st;
  char cbuf[IRC_NLINES][IRC_MAXLEN];
  int cpos;
  int cn;
};

struct conn;
struct event;
struct epoll_cont;

void irc_conn_init(struct epoll_cont*, const char*, uint16_t);
int irc_conn_read(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_irc_msg(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_unix_msg(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_unix_acc(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_close(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_after_read(struct epoll_cont*, uint32_t, struct event*);
