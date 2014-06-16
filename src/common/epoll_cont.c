#include "common/epoll_cont.h"
#include "common/event.h"
#include "common/common.h"

#include <sys/epoll.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void epoll_close_conn(struct epoll_cont *, struct conn *);

void epoll_cont_init(struct epoll_cont *e)
{
	memset(e, 0, sizeof(*e));
	e->cont = 1;
	e->epfd = epoll_create1(EPOLL_CLOEXEC);
	if (e->epfd < 0)
		die("epoll_create1");

	for (int i = 0; i < MAX_CONN; ++i) {
		e->conns[i].rfd = e->conns[i].wfd = -1;
	}
}

void epoll_cont_serve(struct epoll_cont *e)
{
	struct epoll_event es[MAX_EVENTS];

	while (e->cont) {
		int nfd = epoll_wait(e->epfd, es, MAX_EVENTS, -1);
		if (nfd < 0)
			die("epoll_wait");

		for (int i = 0; i < nfd; ++i) {
			struct conn *c = es[i].data.ptr;
			if (es[i].events & (EPOLLHUP | EPOLLERR)) {
				epoll_close_conn(e, c);
			} else if (es[i].events & EPOLLIN) {
				if (c->rfd == -1)
					die2("EPOLLIN on rfd -1");
				if (!c->cbs[EV_READ] (e, c, NULL)) {
					epoll_close_conn(e, c);
				}
			} else if (es[i].events & EPOLLOUT) {
				if (c->wfd == -1)
					die2("EPOLLOUT on wfd -1");
				if (!c->cbs[EV_WRITE] (e, c, NULL)) {
					epoll_close_conn(e, c);
				}
			} else {
				die2("unhandled epoll event");
			}
		}
	}
}

void epoll_cont_walk(struct epoll_cont *e, struct event *ev)
{
	for (size_t p = 0; p < MAX_CONN; ++p) {
		struct conn *c = &e->conns[p];
		if (c->rfd == -1)
			continue;
		event_cb cb = c->cbs[ev->type];
		if (!cb)
			continue;
		if (!cb(e, c, ev))
			epoll_close_conn(e, c);
	}
}

void epoll_cont_destroy(struct epoll_cont *e)
{
	close(e->epfd);
}

void epoll_close_conn(struct epoll_cont *e, struct conn *c)
{
	log("closing connection %d", c->rfd);
	event_cb cb = c->cbs[EV_CLOSE];
	if (cb) {
		e->cont &= cb(e, c, NULL);
	} else {
		log("connection %f does not have cleanup cb", c->rfd);
		e->cont = 0;
	}
}

struct conn *epoll_cont_find_free(struct epoll_cont *e)
{
	for (int p = 0; p < MAX_CONN; ++p)
		if (e->conns[p].rfd == -1)
			return &e->conns[p];
	return NULL;
}

struct conn *epoll_cont_add(struct epoll_cont *e, int rfd, int wfd)
{
	struct conn *c = conn_init(e, rfd, wfd);
	if (!c)
		return NULL;
	c->rfd = rfd;
	c->wfd = wfd;
	c->cbs[EV_CLOSE] = conn_close;
	c->cbs[EV_READ] = conn_read;
	c->cbs[EV_WRITE] = conn_write;
	return c;
}
