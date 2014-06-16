#pragma once

#include "common/conn.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct irc_row {
	time_t ts;
	char s[IRC_MAXLEN];
};

struct irc_data {
	int st;
	struct irc_row cbuf[IRC_NLINES];
	int cpos;
	int cn;
};

struct conn;
struct event;
struct epoll_cont;

void irc_conn_init(struct epoll_cont *, const char *, uint16_t);
int irc_conn_read(struct epoll_cont *, struct conn *, struct event *);
int irc_conn_irc_msg(struct epoll_cont *, struct conn *, struct event *);
int irc_conn_unix_msg(struct epoll_cont *, struct conn *, struct event *);
int irc_conn_unix_acc(struct epoll_cont *, struct conn *, struct event *);
int irc_conn_close(struct epoll_cont *, struct conn *, struct event *);
int irc_conn_after_read(struct epoll_cont *, struct conn *, struct event *);
size_t irc_conn_pack_row(uint8_t *, struct irc_row *);
