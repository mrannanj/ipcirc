#include "common/common.h"
#include "common/iirc.pb-c.h"
#include "common/epoll_cont.h"
#include "client/attach_conn.h"
#include "client/screen_conn.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>

int attach_conn_add(struct epoll_cont* e, const char* hn) {
  int slot = -1;
  int to[2];
  int from[2];
  if (pipe(to) < 0) die("pipe");
  if (pipe(from) < 0) die("pipe");

  pid_t pid = fork();
  if (pid < 0) die("fork");
  if (pid == 0) {
    attach_conn_exec(hn, to, from);
  } else {
    if (close(to[0]) < 0) die("close");
    if (close(from[1]) < 0) die("close");
    slot = epoll_cont_add(e, from[0], to[1]);
    if (slot < 0) die2("no connection slot for child pipe");
    struct conn* c = &e->conns[slot];
    c->cbs[EV_WRITE] = conn_write;
    c->cbs[EV_READ] = conn_read;
    c->cbs[EV_AFTER_READ] = attach_conn_after_read;
    c->cbs[EV_CLOSE] = attach_conn_close;
  }
  return slot;
}

int attach_conn_close(struct epoll_cont* e, uint32_t p, struct event* ev) {
  conn_close(e, p, ev);
  struct screen* s = e->ptr;
  if (s) s->attach_slot = -1;
  return 0;
}

void attach_conn_exec(const char* host, int to[2], int from[2]) {
  const char cmd[] = "iirc-attach";
  if (dup2(to[0], STDIN_FILENO) < 0) die("dup2");
  if (dup2(from[1], STDOUT_FILENO) < 0) die("dup2");
  if (host) {
    if (execlp("ssh", "ssh", host, cmd, (char*)NULL) < 0)
      die("execlp");
  } else {
    if (execlp("strace", "strace", "-otrace", cmd, (char*)NULL) < 0)
      die("execlp");
  }
}

int attach_conn_after_read(struct epoll_cont* e, uint32_t p, struct event* ev) {
  struct conn* c = &e->conns[p];

  while(1) {
    if (c->in_pos < (ssize_t)sizeof(uint16_t)) break;
    uint16_t len;
    memcpy(&len, c->in_buf, sizeof(uint16_t));
    len = ntohs(len);
    if (c->in_pos+sizeof(uint16_t) < len) break;
    AMessage* amsg = amessage__unpack(NULL, len, (uint8_t*)&c->in_buf[2]);
    if (amsg && amsg->type == MESSAGE_TYPE__ROW) {
      screen_add_line(e->ptr, amsg->row->text);
    }
    amessage__free_unpacked(amsg, NULL);
    c->in_pos -= len + sizeof(uint16_t);
    memmove(c->in_buf, &c->in_buf[len + sizeof(uint16_t)], c->in_pos);
  }
  return 1;
}
