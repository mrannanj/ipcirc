#include <ncurses.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "client/screen_conn.h"
#include "common/common.h"
#include "common/iirc.pb-c.h"
#include "common/epoll_cont.h"

void screen_init(struct screen *s)
{
	int max_row, max_col;

	memset(s, 0, sizeof(*s));

	initscr();
	curs_set(1);
	raw();
	noecho();

	getmaxyx(stdscr, max_row, max_col);

	s->text = newwin(max_row - 2, max_col, 0, 0);
	scrollok(s->text, TRUE);
	idlok(s->text, TRUE);
	s->input = newwin(2, max_col, max_row - 2, 0);

	s->cn = 0;
	s->cpos = 0;

	s->cursor_pos = 0;
	s->line_len = 0;
	s->mode = MODE_NORMAL;
	s->attach_conn = NULL;
}

void screen_draw(struct screen *s)
{
	int max_row, max_col;

	erase();
	werase(s->text);
	werase(s->input);
	setsyx(0, 0);

	getmaxyx(stdscr, max_row, max_col);

	wresize(s->text, max_row - 2, max_col);
	mvwin(s->text, 0, 0);

	wresize(s->input, 2, max_col);
	mvwin(s->text, max_row - 2, 0);

	screen_draw_buf(s);
	wrefresh(s->text);

	setsyx(max_row - 2, 0);
	screen_status_bar(s, max_col);

	for (int c = 0; c < max_col && s->line[c]; ++c)
		waddch(s->input, s->line[c]);
	wmove(s->input, 1, s->cursor_pos);
	wrefresh(s->input);
}

void screen_status_bar(struct screen *s, int max_col)
{
	char status[SCR_NCOL];
	time_t t = time(NULL);
	struct tm local;
	ssize_t n;

	localtime_r(&t, &local);
	n = strftime(status, sizeof(status), "%H:%M:%S", &local);
	memmove(&status[max_col - n], status, n);
	memset(status, ' ', max_col - n);
	if (s->mode == MODE_INSERT) {
		n = snprintf(status, sizeof(status), "INSERT");
	} else if (s->mode == MODE_NORMAL) {
		n = snprintf(status, sizeof(status), "NORMAL");
	} else if (s->mode == MODE_REPLACE) {
		n = snprintf(status, sizeof(status), "REPLACE");
	} else {
		die2("invalid mode in screen");
	}
	status[n] = ' ';
	for (int i = 0; i < max_col; ++i) {
		waddch(s->input, status[i]);
	}
}

void screen_add_line(struct screen *s, AMessage * m)
{
	size_t len;

	assert(m->type == MESSAGE_TYPE__ROW);

	len = strlen(m->row->text);
	memcpy(s->cbuf[s->cpos].s, m->row->text, len);
	s->cbuf[s->cpos].s[len] = '\0';
	s->cbuf[s->cpos].ts = m->row->timestamp;

	s->cn = min(s->cn + 1, IRC_NLINES);
	s->cpos = (IRC_NLINES + s->cpos + 1) % IRC_NLINES;
	screen_draw(s);
}

void screen_draw_buf(struct screen *s)
{
	char tbuf[10];
	char buf[1024];

	for (ssize_t i = 0, p = (IRC_NLINES + s->cpos - s->cn) % IRC_NLINES;
	     i < s->cn; ++i, p = (IRC_NLINES + p + 1) % IRC_NLINES) {
		time_t t = s->cbuf[p].ts;
		struct tm local;

		localtime_r(&t, &local);
		strftime(tbuf, sizeof(tbuf), "%H:%M:%S ", &local);
		snprintf(buf, sizeof(buf), "%s%s", tbuf, s->cbuf[p].s);
		waddch(s->text, '\n');

		for (ssize_t j = 0; isprint(buf[j]); ++j) {
			waddch(s->text, buf[j]);
		}
	}
}

void screen_destroy(struct screen *s)
{
	erase();
	endwin();
}

struct conn *screen_conn_add(struct epoll_cont *e)
{
	struct conn *con = epoll_cont_find_free(e);

	if (!con)
		die2("no connection slot for stdin");

	memset(con, 0, sizeof(*con));
	con->rfd = STDIN_FILENO;
	con->wfd = -1;
	con->cbs[EV_READ] = screen_conn_read;
	con->cbs[EV_CLOSE] = conn_close_fatal;
	struct epoll_event ee = {.events = EPOLLIN,.data.ptr = con };

	if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, con->rfd, &ee) < 0)
		die("epoll_ctl");
	return con;
}

void screen_conn_push_line(struct epoll_cont *e, struct conn *c)
{
	struct screen *s = e->ptr;
	char *line = s->line;

	if (s->attach_conn != NULL) {
		line[s->line_len] = '\n';
		line[s->line_len + 1] = '\0';
		conn_write_buf2(e, s->attach_conn, line, s->line_len + 1);
	}
	s->cursor_pos = s->line_len = 0;
	memset(s->line, 0, SCR_NCOL);
}

int screen_conn_read(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	char ch;
	ssize_t nread = read(c->rfd, &ch, 1);
	struct screen *s;
	int cont;

	if (nread < 0) {
		log_errno("read");
		return 0;
	} else if (nread == 0) {
		log("EOF from stdin");
		return 0;
	}

	s = e->ptr;
	if (!s)
		die2("screen is NULL");

	cont = 1;
	if (s->mode == MODE_REPLACE) {
		if (isprint(ch)) {
			s->line[s->cursor_pos] = ch;
			s->cursor_pos = min(s->cursor_pos + 1, SCR_NCOL - 1);
			s->line_len = max(s->cursor_pos, s->line_len);
		} else if (ch == ASCII_ESC) {
			s->mode = MODE_NORMAL;
		}
	} else if (s->mode == MODE_INSERT) {
		if (isprint(ch)) {
			s->line_len = min(s->line_len + 1, SCR_NCOL - 1);
			if (s->cursor_pos < s->line_len) {
				memmove(&s->line[s->cursor_pos + 1],
					&s->line[s->cursor_pos],
					s->line_len - (s->cursor_pos + 1));
			}
			s->line[s->cursor_pos] = ch;
			s->cursor_pos = min(s->cursor_pos + 1, s->line_len);
		} else if (ch == ASCII_ESC) {
			s->cursor_pos = max(0, s->cursor_pos - 1);
			s->mode = MODE_NORMAL;
		} else if (ch == '\r') {
			screen_conn_push_line(e, c);
		}
	} else if (s->mode == MODE_NORMAL) {
		switch (ch) {
		case '\r':
			screen_conn_push_line(e, c);
			break;
		case 'x':
			if (s->cursor_pos < s->line_len) {
				memmove(&s->line[s->cursor_pos],
					&s->line[s->cursor_pos + 1],
					s->line_len - s->cursor_pos);
				s->line_len = max(0, s->line_len - 1);
				s->cursor_pos =
				    min(max(0, s->line_len - 1), s->cursor_pos);
			}
			break;
		case 'I':
			s->cursor_pos = 0;
		case 'i':
			s->mode = MODE_INSERT;
			break;
		case 'R':
			s->mode = MODE_REPLACE;
			break;
		case 'l':
			s->cursor_pos =
			    min(max(s->line_len - 1, 0), s->cursor_pos + 1);
			break;
		case 'h':
			s->cursor_pos = max(0, s->cursor_pos - 1);
			break;
		case 'A':
			s->cursor_pos = s->line_len;
			s->mode = MODE_INSERT;
		case 'a':
			s->cursor_pos = min(s->line_len, s->cursor_pos + 1);
			s->mode = MODE_INSERT;
			break;
		case 'q':
			cont = 0;
			break;
		}
	}
	screen_draw(s);

	return cont;
}
