#include "common/epoll_cont.h"
#include "common/irc_conn.h"
#include "common/common.h"
#include "common/conn.h"
#include "common/iirc.pb-c.h"

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

static void irc_send(struct epoll_cont *, struct conn *, const char *, ...);
static void irc_send_handshake(struct epoll_cont *, struct conn *);

#define ST_INIT 0
#define ST_AFTER_FIRST 1

int irc_conn_irc_msg(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	if (c != ev->source)
		return 1;
	struct irc_data *irc = c->data.ptr;
	char *msg = ev->p;
	int n = 0;
	char server_addr[513];

	switch (irc->st) {
	case ST_INIT:
		irc_send_handshake(e, c);
		irc->st = ST_AFTER_FIRST;
		break;
	case ST_AFTER_FIRST:
		n = sscanf(msg, "PING :%s", server_addr);
		if (n != 1)
			break;
		irc_send(e, c, "PONG :%s\r\n", server_addr);
		goto out;
	default:
		log("unkown state in irc_conn");
	}
	printf("%s", msg);
	memcpy(irc->cbuf[irc->cpos].s, msg, strlen(msg) + 1);
	irc->cbuf[irc->cpos].ts = time(NULL);

	irc->cpos = (irc->cpos + 1) % IRC_NLINES;
	irc->cn = min(IRC_NLINES, irc->cn + 1);
 out:
	return 1;
}

size_t irc_conn_pack_row(uint8_t * buf, struct irc_row * r)
{
	Row row;
	row__init(&row);
	row.timestamp = r->ts;
	row.text = r->s;
	AMessage amessage;
	amessage__init(&amessage);
	amessage.type = MESSAGE_TYPE__ROW;
	amessage.row = &row;
	uint16_t len = amessage__get_packed_size(&amessage);
	uint16_t nlen = htons(len);
	memcpy(buf, &nlen, sizeof(nlen));
	amessage__pack(&amessage, &buf[sizeof(nlen)]);
	return len + sizeof(nlen);
}

int irc_conn_unix_acc(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	struct irc_data *irc = c->data.ptr;
	int k = IRC_NLINES + irc->cpos - irc->cn;
	for (int j = 0; j < irc->cn; ++j, k = (k + 1) % IRC_NLINES) {
		uint8_t buf[2048];
		size_t total_len = irc_conn_pack_row(buf, &irc->cbuf[k]);
		conn_write_buf2(e, ev->source, (char *)buf, total_len);
	}
	return 1;
}

int irc_conn_unix_msg(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	char buf[IRC_MAXLEN];
	ssize_t len = strlen(ev->p);
	memcpy(buf, ev->p, len);
	buf[len - 1] = '\r';
	buf[len] = '\n';
	buf[len + 1] = '\0';
	len += 1;
	conn_write_buf2(e, c, buf, len);
	return 1;
}

int irc_conn_close(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	if (c->data.ptr)
		free(c->data.ptr);
	conn_close(e, c, ev);
	return 1;
}

void irc_conn_init(struct epoll_cont *e, const char *host, uint16_t port)
{
	int rfd = tcp_conn_init(host, port);
	int wfd = dup(rfd);
	struct conn *c = conn_init(e, rfd, wfd);
	if (!c)
		die2("no slot for irc connection");
	c->data.ptr = malloc(sizeof(struct irc_data));
	assert(c->data.ptr);
	struct irc_data *irc = c->data.ptr;
	irc->st = ST_INIT;
	c->cbs[EV_CLOSE] = irc_conn_close;
	c->cbs[EV_READ] = conn_read;
	c->cbs[EV_AFTER_READ] = irc_conn_after_read;
	c->cbs[EV_WRITE] = conn_write;
	c->cbs[EV1_IRC_MESSAGE] = irc_conn_irc_msg;
	c->cbs[EV1_UNIX_MESSAGE] = irc_conn_unix_msg;
	c->cbs[EV1_UNIX_ACCEPTED] = irc_conn_unix_acc;
	irc->cpos = 0;
	irc->cn = 0;
}

int irc_conn_after_read(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	char prev_ch = '\0';
	char msg[IRC_MAXLEN];

	for (ssize_t i = 0; i < c->in_pos;) {
		const char x = c->in_buf[i];
		switch (prev_ch) {
		case '\r':
			if (x != '\n')
				break;
			memcpy(msg, c->in_buf, i + 1);
			msg[i + 1] = '\0';
			struct event evt = {.type = EV1_IRC_MESSAGE,.source =
				    c,.p = msg };
			epoll_cont_walk(e, &evt);
			c->in_pos -= i + 1;
			memmove(c->in_buf, &c->in_buf[i + 1], c->in_pos);
			i = 0;
			break;
		default:
			++i;
			break;
		}
		prev_ch = x;
	}
	return 1;
}

void irc_send(struct epoll_cont *e, struct conn *c, const char *message, ...)
{
	va_list vl;
	va_start(vl, message);

	char buf[IRC_MAXLEN];
	int n = vsnprintf(buf, IRC_MAXLEN - 3, message, vl);
	if (n < 0) {
		log("vsnprintf error");
		return;
	}
	va_end(vl);

	buf[n] = '\r';
	buf[n + 1] = '\n';
	buf[n + 2] = '\0';
	conn_write_buf2(e, c, buf, n + 1);
}

void irc_send_handshake(struct epoll_cont *e, struct conn *c)
{
	char nick[] = "iirctest";
	irc_send(e, c, "NICK %s", nick);
	irc_send(e, c, "USER %s 8 * :%s", nick, nick);
	irc_send(e, c, "MODE %s +i", nick);
}
