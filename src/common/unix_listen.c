#include "common/unix_listen.h"
#include "common/unix_conn.h"
#include "common/epoll_cont.h"
#include "common/event.h"
#include "common/common.h"

#include <unistd.h>
#include <sys/param.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

static const size_t UNIX_LISTEN_BACKLOG = 10;

int unix_listen_read(struct epoll_cont *e, struct conn *c, struct event *ev)
{
	int client = unix_listen_accept(c->rfd);
	if (client < 0) {
		log("didn't accept unix connection");
		return 1;
	}
	struct conn *nc = unix_conn_init(e, client);
	if (nc) {
		struct event evt = {.type = EV1_UNIX_ACCEPTED,.source = nc };
		epoll_cont_walk(e, &evt);
	}
	return 1;
}

int unix_listen_init()
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		die("socket");
	size_t path_len = strlen(UNIX_ADDR_PREFIX);
	struct sockaddr_un sa = {.sun_family = AF_UNIX };
	memcpy(sa.sun_path, UNIX_ADDR_PREFIX, path_len);
	sa.sun_path[0] = '\0';
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa.sun_family) + path_len) <
	    0)
		die("bind");
	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0)
		die("setsockopt");
	if (listen(fd, UNIX_LISTEN_BACKLOG) < 0)
		die("listen");
	return fd;
}

int unix_listen_accept(int s)
{
	struct sockaddr_un client;
	socklen_t addrlen = sizeof(client);
	int acc = accept(s, (struct sockaddr *)&client, &addrlen);
	if (acc < 0) {
		log_errno("accept");
		return -1;
	} else if (!unix_conn_verify_cred(acc)) {
		log("invalid credentials");
		close(acc);
		return -1;
	}
	return acc;
}
