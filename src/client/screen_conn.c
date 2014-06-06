#include <ncurses.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "client/screen_conn.h"
#include "common/common.h"
#include "common/epoll_cont.h"

void screen_init(struct screen* s) {
  memset(s, 0, sizeof(*s));

  initscr();
  curs_set(1);
  raw();
  noecho();

  s->cn = 0;
  s->cpos = 0;

  s->cursor_pos = 0;
  s->line_len = 0;
  s->mode = MODE_NORMAL;
  s->attach_slot = -1;
}

void screen_draw(struct screen* s) {
  erase();
  int max_row, max_col;
  getmaxyx(stdscr, max_row, max_col);
  setsyx(0, 0);
  screen_draw_buf(s, max_row-2, max_col);
  setsyx(max_row-2, 0);
  screen_status_bar(s, max_row-2, max_col);
  for (int c = 0; c < max_col && s->line[c]; ++c)
    addch(s->line[c]);
  move(max_row-1, s->cursor_pos);
  refresh();
}

void screen_status_bar(struct screen* s, int row, int max_col) {
  char status[SCR_NCOL];
  time_t t = time(NULL);
  struct tm local;
  localtime_r(&t, &local);
  ssize_t n = strftime(status, sizeof(status), "%H:%M:%S", &local);
  memmove(&status[max_col-n], status, n);
  memset(status, ' ', max_col-n);
  if (s->mode == MODE_INSERT) {
    n = snprintf(status, sizeof(status), "INSERT");
  } else if (s->mode == MODE_NORMAL) {
    n = snprintf(status, sizeof(status), "NORMAL");
  } else if (s->mode == MODE_REPLACE) {
    n = snprintf(status, sizeof(status), "REPLACE");
  } else {
    die2("invalid mode");
  }
  status[n] = ' ';
  setsyx(row, 0);
  for (int i = 0; i < max_col; ++i) {
    addch(status[i]);
  }
}

void screen_add_line(struct screen* s, char* row) {
  size_t len = strlen(row);
  memcpy(s->cbuf[s->cpos], row, len);
  s->cbuf[s->cpos][len] = '\0';
  s->cn = min(s->cn + 1, IRC_NLINES);
  s->cpos = (IRC_NLINES + s->cpos + 1) % IRC_NLINES;
  screen_draw(s);
}

void screen_draw_buf(struct screen* s, ssize_t row, ssize_t col) {
  ssize_t i = 0;
  ssize_t sp = 0;
  for (ssize_t p = s->cpos; i < row;
      ++i, p = (IRC_NLINES + p - 1) % IRC_NLINES)
  {
    sp += 1 + strlen(s->cbuf[p])/col;
  }
  i = 0;
  for (ssize_t p = (IRC_NLINES + s->cpos - sp) % IRC_NLINES;
      i < row; ++i, p = (IRC_NLINES + p + 1) % IRC_NLINES)
  {
    ssize_t len = strlen(s->cbuf[p]);
    for (ssize_t j = 0; j < len; ++j) {
      char c = s->cbuf[p][j];
      if (isprint(c)) {
        addch(c);
      } else {
        break;
      }
    }
    addch('\n');
  }
}

void screen_destroy(struct screen* s) {
  erase();
  endwin();
}

int screen_conn_add(struct epoll_cont* e) {
  int slot = epoll_cont_find_free(e);
  if (slot < 0) die2("no connection slot for stdin");
  struct conn* c = &e->conns[slot];
  memset(c, 0, sizeof(*c));
  c->rfd = STDIN_FILENO;
  c->cbs[EV_READ] = screen_conn_read;
  c->cbs[EV_CLOSE] = conn_close_fatal;
  struct epoll_event ee = { .events = EPOLLIN, .data.u32 = slot };
  if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->rfd, &ee) < 0)
    die("epoll_ctl");
  return slot;
}

void screen_conn_push_line(struct epoll_cont* e, uint32_t p) {
  struct screen* s = e->ptr;
  char* line = s->line;
  if (s->attach_slot != -1) {
    line[s->line_len] = '\n';
    line[s->line_len+1] = '\0';
    conn_write_buf2(e, s->attach_slot, line, s->line_len+1);
  }
  s->cursor_pos = s->line_len = 0;
  memset(s->line, 0, SCR_NCOL);
}

int screen_conn_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  char c;
  ssize_t nread = read(e->conns[p].rfd, &c, 1);
  if (nread < 0) {
    log_errno("read");
    return 0;
  } else if (nread == 0) {
    log("EOF from stdin");
    return 0;
  }
  struct screen* s = e->ptr;
  if (!s) die2("screen is NULL");

  int cont = 1;
  if (s->mode == MODE_REPLACE) {
    if (isprint(c)) {
      s->line[s->cursor_pos] = c;
      s->cursor_pos = min(s->cursor_pos + 1, SCR_NCOL - 1);
      s->line_len = max(s->cursor_pos, s->line_len);
    } else if (c == ASCII_ESC) {
      s->mode = MODE_NORMAL;
    }
  } else if (s->mode == MODE_INSERT) {
    if (isprint(c)) {
      s->line_len = min(s->line_len + 1, SCR_NCOL - 1);
      if (s->cursor_pos < s->line_len) {
        memmove(&s->line[s->cursor_pos+1], &s->line[s->cursor_pos],
            s->line_len-(s->cursor_pos+1));
      }
      s->line[s->cursor_pos] = c;
      s->cursor_pos = min(s->cursor_pos + 1, s->line_len);
    } else if (c == ASCII_ESC) {
      s->cursor_pos = max(0, s->cursor_pos - 1);
      s->mode = MODE_NORMAL;
    } else if (c == '\r') {
      screen_conn_push_line(e, p);
    }
  } else if (s->mode == MODE_NORMAL) {
    switch (c) {
      case '\r':
        screen_conn_push_line(e, p);
        break;
      case 'x':
        if (s->cursor_pos < s->line_len) {
          memmove(&s->line[s->cursor_pos], &s->line[s->cursor_pos+1],
              s->line_len - s->cursor_pos);
          s->line_len = max(0, s->line_len-1);
          s->cursor_pos = min(max(0, s->line_len-1), s->cursor_pos);
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
        s->cursor_pos = min(max(s->line_len-1, 0), s->cursor_pos + 1);
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
