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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct conn *attach_conn_add(struct epoll_cont *e, const char *hn)
{
	struct conn *c = NULL;
	int to[2];
	int from[2];
	if (pipe(to) < 0)
		die("pipe");
	if (pipe(from) < 0)
		die("pipe");

	pid_t pid = fork();
	if (pid < 0)
		die("fork");
	if (pid == 0) {
		attach_conn_exec(hn, to, from);
	} else {
		if (close(to[0]) < 0)
			die("close");
		if (close(from[1]) < 0)
			die("close");
		c = epoll_cont_add(e, from[0], to[1]);
		if (!c)
			die2("no connection slot for child pipe");
		c->cbs[EV_WRITE] = conn_write;
		c->cbs[EV_READ] = conn_read;
		c->cbs[EV_AFTER_READ] = attach_conn_after_read;
		c->cbs[EV_CLOSE] = attach_conn_close;
	}
	return c;
}

int attach_conn_close(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	conn_close(e, c, ev);
	struct screen *s = e->ptr;
	if (s)
		s->attach_conn = NULL;
	return 0;
}

void attach_conn_exec(const char *host, int to[2], int from[2])
{
	const char cmd[] = "iirc-attach";
	int null = open("/dev/null", O_WRONLY);
	if (null < 0)
		die("open");
	if (dup2(null, STDERR_FILENO) < 0)
		die("dup2");
	if (dup2(to[0], STDIN_FILENO) < 0)
		die("dup2");
	if (dup2(from[1], STDOUT_FILENO) < 0)
		die("dup2");
	if (host) {
		if (execlp("ssh", "ssh", host, cmd, (char *)NULL) < 0)
			die("execlp");
	} else {
		if (execlp(cmd, cmd, (char *)NULL) < 0)
			die("execlp");
	}
}

int attach_conn_after_read(struct epoll_cont *e, struct conn *c,
			   struct event *ev)
{
	while (c->in_pos >= (ssize_t) sizeof(uint16_t)) {
		uint16_t len;
		memcpy(&len, c->in_buf, sizeof(uint16_t));
		len = ntohs(len);
		if (c->in_pos + sizeof(uint16_t) < len)
			break;
		AMessage *m =
		    amessage__unpack(NULL, len, (uint8_t *) & c->in_buf[2]);
		if (m && m->type == MESSAGE_TYPE__ROW) {
			screen_add_line(e->ptr, m);
		}
		amessage__free_unpacked(m, NULL);
		c->in_pos -= len + sizeof(uint16_t);
		memmove(c->in_buf, &c->in_buf[len + sizeof(uint16_t)],
			c->in_pos);
	}
	return 1;
}
