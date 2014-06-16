#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <assert.h>
#include <signal.h>
#include <sys/prctl.h>

#include "common/conn.h"
#include "common/unix_conn.h"
#include "common/common.h"
#include "common/epoll_cont.h"

struct conn *add_unix_conn(struct epoll_cont *e)
{
	char path[UNIX_PATH_MAX];
	size_t len;
	int us, on;
	struct sockaddr_un sa;
	struct conn *con;

	if (!find_server_addr(path, &len))
		die2("backend is not running");

	us = socket(AF_UNIX, SOCK_STREAM, 0);
	if (us < 0)
		die("socket");

	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, len);
	sa.sun_path[0] = '\0';

	on = 1;
	if (setsockopt(us, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0)
		die("setsockopt");
	if (connect(us, (struct sockaddr *)&sa, sizeof(sa.sun_family) + len) <
	    0)
		die("connect");
	if (!unix_conn_verify_cred(us))
		die2("server has invalid credentials");

	con = unix_conn_init(e, us);
	if (!con)
		die2("creating unix connection failed");
	con->cbs[EV_CLOSE] = conn_close_fatal;
	con->cbs[EV_AFTER_READ] = conn_write_to_slot;
	con->cbs[EV_READ] = conn_read;
	con->cbs[EV_WRITE] = conn_write;
	return con;
}

struct conn *add_stdio(struct epoll_cont *e)
{
	struct conn *con = epoll_cont_add(e, STDIN_FILENO, STDOUT_FILENO);

	if (!con)
		die2("failed to add stdin");
	con->cbs[EV_AFTER_READ] = conn_write_to_slot;
	con->cbs[EV_CLOSE] = conn_close_fatal;

	return con;
}

int main(void)
{
	struct epoll_cont e;
	epoll_cont_init(&e);
	struct conn *c1 = add_unix_conn(&e);
	struct conn *c2 = add_stdio(&e);

	c1->data.ptr = c2;
	c2->data.ptr = c1;
	epoll_cont_serve(&e);
	epoll_cont_destroy(&e);
}
