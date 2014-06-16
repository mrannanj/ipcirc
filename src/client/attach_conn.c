#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common/common.h"
#include "common/iirc.pb-c.h"
#include "common/epoll_cont.h"
#include "client/attach_conn.h"
#include "client/screen_conn.h"

struct conn *attach_conn_add(struct epoll_cont *e, const char *hn)
{
	struct conn *con = NULL;
	int to[2];
	int from[2];
	pid_t pid;

	if (pipe(to) < 0)
		die("pipe");
	if (pipe(from) < 0)
		die("pipe");

	pid = fork();
	if (pid < 0)
		die("fork");
	if (pid == 0) {
		attach_conn_exec(hn, to, from);
	} else {
		if (close(to[0]) < 0)
			die("close");
		if (close(from[1]) < 0)
			die("close");

		con = epoll_cont_add(e, from[0], to[1]);
		if (!con)
			die2("no connection slot for child pipe");
		con->cbs[EV_WRITE] = conn_write;
		con->cbs[EV_READ] = conn_read;
		con->cbs[EV_AFTER_READ] = attach_conn_after_read;
		con->cbs[EV_CLOSE] = attach_conn_close;
	}

	return con;
}

int attach_conn_close(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	struct screen *s;

	conn_close(e, c, ev);
	s = e->ptr;
	if (s)
		s->attach_conn = NULL;

	return 0;
}

void attach_conn_exec(const char *host, int to[2], int from[2])
{
	const char cmd[] = "iirc-attach";
	int devnull = open("/dev/null", O_WRONLY);

	if (devnull < 0)
		die("open");
	if (dup2(devnull, STDERR_FILENO) < 0)
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

int attach_conn_after_read(struct epoll_cont *e, struct conn *con,
			   struct event *ev)
{
	while (con->in_pos >= (ssize_t) sizeof(uint16_t)) {
		uint16_t len;

		memcpy(&len, con->in_buf, sizeof(uint16_t));
		len = ntohs(len);
		if (con->in_pos + sizeof(uint16_t) < len)
			break;
		AMessage *m =
		    amessage__unpack(NULL, len, (uint8_t *) & con->in_buf[2]);
		if (m && m->type == MESSAGE_TYPE__ROW) {
			screen_add_line(e->ptr, m);
		}
		amessage__free_unpacked(m, NULL);
		con->in_pos -= len + sizeof(uint16_t);
		memmove(con->in_buf, &con->in_buf[len + sizeof(uint16_t)],
			con->in_pos);
	}

	return 1;
}
