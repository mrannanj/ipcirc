#pragma once

#include <stddef.h>
#include <stdint.h>

#define IRC_NLINES 5
#define IRC_MAXLEN 513

typedef struct {
  int state;
  size_t pos;
  char* buf;
  char (*last)[IRC_NLINES][IRC_MAXLEN];
} irc_conn;

struct conn;
struct event;
struct epoll_cont;

void irc_conn_init(struct conn*, const char*, uint16_t);
void irc_conn_read(struct epoll_cont*, uint32_t, struct event*);
void irc_conn_irc_msg(struct epoll_cont*, uint32_t, struct event*);
void irc_conn_unix_msg(struct epoll_cont*, uint32_t, struct event*);
