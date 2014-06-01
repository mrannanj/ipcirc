#pragma once

#include <stddef.h>
#include <stdint.h>

#define IRC_NLINES 20
#define IRC_MAXLEN 513

typedef struct {
  int state;
  size_t pos;
  char* buf;
  char (*cbuf)[IRC_MAXLEN];
  int cpos;
  int cn;
} irc_conn;

struct conn;
struct event;
struct epoll_cont;

void irc_conn_init(struct conn*, const char*, uint16_t);
int irc_conn_read(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_irc_msg(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_unix_msg(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_unix_acc(struct epoll_cont*, uint32_t, struct event*);
int irc_conn_close(struct epoll_cont*, uint32_t, struct event*);
