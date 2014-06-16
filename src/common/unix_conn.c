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

#include "common/epoll_cont.h"
#include "common/event.h"
#include "common/unix_conn.h"
#include "common/irc_conn.h"
#include "common/common.h"
#include "common/conn.h"

struct conn *unix_conn_init(struct epoll_cont *e, int rfd)
{
	int wfd = dup(rfd);
	struct conn *c = conn_init(e, rfd, wfd);
	if (!c) {
		log("connection slots used, not accepting unix connection %d",
		    rfd);
		close(rfd);
		close(wfd);
		return NULL;
	}
	c->cbs[EV_READ] = conn_read;
	c->cbs[EV_WRITE] = conn_write;
	c->cbs[EV_AFTER_READ] = unix_conn_after_read;
	c->cbs[EV1_IRC_MESSAGE] = unix_conn_irc_msg;
	c->cbs[EV_CLOSE] = conn_close;
	return c;
}

int unix_conn_irc_msg(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	if (strncmp(ev->p, "PING", 4) == 0)
		return 1;
	uint8_t buf[2048];
	size_t total_len = irc_conn_pack_row(buf, ev->p);
	conn_write_buf2(e, c, (char *)buf, total_len);
	return 1;
}

int unix_conn_after_read(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	char msg[IRC_MAXLEN];
	for (ssize_t i = 0; i < c->in_pos;) {
		const char x = c->in_buf[i];
		switch (x) {
		case '\n':
			memcpy(msg, c->in_buf, i + 1);
			msg[i + 1] = '\0';
			struct event evt = {.type = EV1_UNIX_MESSAGE,.source =
				    c,.p = msg
			};
			epoll_cont_walk(e, &evt);
			c->in_pos -= i + 1;
			memmove(c->in_buf, &c->in_buf[i + 1], c->in_pos);
			i = 0;
			break;
		default:
			++i;
			break;
		}
	}
	return 1;
}

int unix_conn_verify_cred(int fd)
{
	struct ucred cr;
	socklen_t ucred_len = sizeof(struct ucred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &ucred_len) == -1) {
		log_errno("getsockopt");
		return 0;
	}
	return cr.uid == getuid();
}
