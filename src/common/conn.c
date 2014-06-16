#include "common/conn.h"
#include "common/common.h"
#include "common/epoll_cont.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct conn *conn_init(struct epoll_cont *e, int rfd, int wfd)
{
	struct conn *c = epoll_cont_find_free(e);
	if (!c)
		return NULL;
	memset(c, 0, sizeof(*c));

	c->in_pos = 0;
	c->in_buf = malloc(CONN_BUFSIZ);
	assert(c->in_buf);

	c->out_pos = 0;
	c->out_buf = malloc(CONN_BUFSIZ);
	assert(c->out_buf);

	c->rfd = rfd;
	struct epoll_event ee = {
		.events = EPOLLIN | EPOLLHUP | EPOLLERR,
		.data.ptr = c
	};
	if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->rfd, &ee) < 0)
		die("epoll_ctl");

	c->wfd = wfd;
	ee.events = EPOLLHUP | EPOLLERR;
	if (epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->wfd, &ee) < 0)
		die("epoll_ctl");

	return c;
}

int conn_close_fatal(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	conn_close(e, c, ev);
	return 0;
}

int conn_close(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	if (c->rfd != -1) {
		if (epoll_ctl(e->epfd, EPOLL_CTL_DEL, c->rfd, NULL) < 0)
			log_errno("epoll_ctl");
		if (close(c->rfd) < 0)
			log_errno("close");
	}
	if (c->wfd != -1) {
		if (epoll_ctl(e->epfd, EPOLL_CTL_DEL, c->wfd, NULL) < 0)
			log_errno("epoll_ctl");
		if (close(c->wfd) < 0)
			log_errno("close");
	}
	if (c->in_buf)
		free(c->in_buf);
	if (c->out_buf)
		free(c->out_buf);
	memset(c, 0, sizeof(*c));
	c->rfd = c->wfd = -1;
	return 1;
}

int conn_read(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	ssize_t nread =
	    read(c->rfd, &c->in_buf[c->in_pos], CONN_BUFSIZ - c->in_pos);
	if (nread < 0) {
		log_errno("read");
		return 0;
	} else if (nread == 0) {
		log("EOF from rfd %d", c->rfd);
		return 0;
	}
	c->in_pos += nread;
	c->cbs[EV_AFTER_READ] (e, c, NULL);
	return 1;
}

int conn_write(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	ssize_t nwrote = write(c->wfd, c->out_buf, c->out_pos);
	if (nwrote < 0) {
		log_errno("write");
		return 0;
	}
	c->out_pos -= nwrote;
	if (c->out_pos == 0) {
		struct epoll_event ee = {
			.events = EPOLLHUP | EPOLLERR,
			.data.ptr = c
		};
		epoll_ctl(e->epfd, EPOLL_CTL_MOD, c->wfd, &ee);
	} else {
		memmove(c->out_buf, &c->out_buf[nwrote], c->out_pos);
	}
	return 1;
}

int conn_write_to_slot(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	struct conn *target = c->data.ptr;
	if (target->rfd == -1)
		return 0;
	int r = conn_write_buf2(e, target, c->in_buf, c->in_pos);
	c->in_pos = 0;
	return r;
}

int conn_write_buf2(struct epoll_cont *e, struct conn *c, char *b, ssize_t len)
{
	if (len > CONN_BUFSIZ - c->out_pos) {
		log("wfd %d write buffer full", c->wfd);
		return 0;
	}
	memcpy(&c->out_buf[c->out_pos], b, len);
	c->out_pos += len;
	struct epoll_event ee = {
		.events = EPOLLOUT | EPOLLHUP | EPOLLERR,
		.data.ptr = c
	};
	if (epoll_ctl(e->epfd, EPOLL_CTL_MOD, c->wfd, &ee) < 0) {
		log_errno("epoll_ctl");
	}
	return 1;
}
