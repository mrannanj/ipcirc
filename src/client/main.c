#include "common/common.h"
#include "common/iirc.pb-c.h"
#include "common/epoll_cont.h"
#include "client/attach_conn.h"
#include "client/screen_conn.h"

int main(int argc, char **argv)
{
	struct screen scr;
	struct epoll_cont e;

	screen_init(&scr);
	screen_draw(&scr);

	epoll_cont_init(&e);
	e.ptr = &scr;

	scr.attach_conn = attach_conn_add(&e, argv[1]);
	screen_conn_add(&e);

	epoll_cont_serve(&e);
	epoll_cont_destroy(&e);
	screen_destroy(&scr);
}
