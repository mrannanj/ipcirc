#pragma once

#include <ncurses.h>
#include <time.h>

#include "common/epoll_cont.h"
#include "common/iirc.pb-c.h"

#define SCR_NROW 512
#define SCR_NCOL 512

#define MODE_NORMAL 0
#define MODE_INSERT 1
#define MODE_REPLACE 2

#define ASCII_ESC 27

struct row {
	time_t ts;
	char s[IRC_MAXLEN];
};

struct screen {
	char line[SCR_NCOL];
	int line_len;
	int cursor_pos;
	int mode;

	struct row cbuf[IRC_NLINES];
	int cpos;
	int cn;

	WINDOW* text;
	WINDOW* input;

	struct conn* attach_conn;
};

int screen_conn_read(struct epoll_cont*, struct conn*, struct event*);

struct conn* screen_conn_add(struct epoll_cont*);

void screen_init(struct screen*);
void screen_add_line(struct screen*, AMessage*);
void screen_draw(struct screen*);
void screen_destroy(struct screen*);
void screen_draw_buf(struct screen*);
void screen_status_bar(struct screen*, int);
