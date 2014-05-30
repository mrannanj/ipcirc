#include "irc_conn.h"
#include "conn.h"
#include "epoll_cont.h"
#include "common/common.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <assert.h>

static int my_connect_sock(const char*, uint16_t);
static void check_for_messages(struct epoll_cont*, uint32_t, ssize_t);
static void mknick(size_t, char*);
static void send_data(int, const char *, ...);
static void send_init(int);

static const size_t NICK_LEN = 6;
static const int ST_INIT = 0;
static const int ST_FIRST_MSG = 1;

int irc_conn_irc_msg(struct epoll_cont* e, uint32_t p, struct event* ev) {
  irc_conn* i = &e->conns[p].data.irc;
  char* msg = ev->p;
  printf("%s", msg);
  if (i->state == ST_INIT) {
    i->state = ST_FIRST_MSG;
    send_init(e->conns[p].fd);
  }
  return 1;
}

int irc_conn_unix_msg(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  char buf[IRC_MAXLEN];
  ssize_t len = strlen(ev->p);
  memcpy(buf, ev->p, len);
  buf[len-1] = '\r';
  buf[len] = '\n';
  buf[len+1] = '\0';
  len += 1;
  ssize_t nwrote = write(c->fd, buf, len);
  if (nwrote < 0) {
    log_errno("write");
    return 0;
  } else if (len != nwrote) {
    log("partial write on irc_conn %d", c->fd);
    return 0;
  }
  return 1;
}

int irc_conn_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];
  irc_conn* i = &c->data.irc;
  ssize_t nread = read(c->fd, &i->buf[i->pos], IRC_MAXLEN - i->pos);
  if (nread < 0) {
    log_errno("read"); return 0;
  } else if (nread == 0) {
    log("EOF from irc_conn %d", c->fd); return 0;
  }
  check_for_messages(e, p, nread);
  return 1;
}

void irc_conn_init(struct conn* c, const char* host, uint16_t port) {
  c->data.irc.state = ST_INIT;
  c->cbs[EV_READY_TO_READ] = irc_conn_read;
  c->cbs[EV_IRC_MESSAGE] = irc_conn_irc_msg;
  c->cbs[EV_UNIX_MESSAGE] = irc_conn_unix_msg;
  c->data.irc.pos = 0;
  c->data.irc.buf = (char*)malloc(IRC_MAXLEN);
  assert(c->data.irc.buf);
  c->data.irc.last = malloc(IRC_MAXLEN*IRC_NLINES);
  assert(c->data.irc.last);
  c->fd = my_connect_sock(host, port);
}

void send_init(int fd) {
  char nick[NICK_LEN+1];
  char *user = nick;
  mknick(NICK_LEN, nick);
  send_data(fd, "NICK %s", nick);
  send_data(fd, "USER %s 8 * :%s", user, user);
  send_data(fd, "MODE %s +i", nick);
  send_data(fd, "JOIN #ipc-irc");
}

void check_for_messages(struct epoll_cont* e, uint32_t p, ssize_t nread) {
  irc_conn* i = &e->conns[p].data.irc;
  char last = '\0';
  char msg[IRC_MAXLEN];
  for (ssize_t k = 0; k < nread;) {
    char c = i->buf[i->pos+k];
    switch (last) {
      case '\r':
        if (c == '\n') {
          memcpy(msg, i->buf, i->pos+k+1);
          msg[i->pos+k+1] = '\0';
          struct event ev = { .type = EV_IRC_MESSAGE, .source = p, .p = msg };
          epoll_cont_walk(e, &ev);
          memmove(i->buf, &i->buf[i->pos+k+1], nread - (k+1));
          i->pos = 0;
          nread -= k+1;
          k = 0;
        }
        last = '\0';
        break;
      default:
        last = c;
        ++k;
        break;
    }
  }
}

int my_connect_sock(const char *host, uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    log("socket");
    return -1;
  }

  struct sockaddr_in sa;
  struct addrinfo *ai = NULL;
  struct addrinfo hai;
  memset(&hai, 0, sizeof(hai));

  hai.ai_family = AF_INET;
  hai.ai_socktype = SOCK_STREAM;
  int addr_err = getaddrinfo(host, 0, &hai, &ai);
  if (addr_err != 0) {
    log("getaddrinfo: %s", gai_strerror(addr_err));
    goto err;
  }
  memcpy(&sa, ai->ai_addr, sizeof(sa));
  sa.sin_port = htons(port);

  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    log_errno("connect");
    goto err;
  }
  freeaddrinfo(ai);
  return fd;

err:
  close(fd);
  freeaddrinfo(ai);
  return -1;
}

void send_data(int fd, const char *message, ...) {
  va_list vl;
  va_start(vl, message);

  char buf[IRC_MAXLEN];
  int n = vsnprintf(buf, IRC_MAXLEN-3, message, vl);
  if (n < 0) {
    log("vsnprintf error");
    return;
  }
  va_end(vl);

  buf[n] = '\r';
  buf[n+1] = '\n';
  buf[n+2] = '\0';
  n += 2;
  printf("%s", buf);
  ssize_t nwrote = write(fd, buf, n);
  if (nwrote < 0) {
    log_errno("write");
  } else if (nwrote != n) {
    log("partial write on %d", fd);
  }
}

void mknick(size_t len, char* nick) {
  const char pot[] = "_0123456789abcdefghijklmnopqrstuvwxyz";
  size_t pot_len = sizeof(pot)/sizeof(pot[0]);
  nick[0] = 'a';
  for (size_t i = 1; i < len; ++i)
    nick[i] = pot[rand()%pot_len];
  nick[len] = '\0';
}
