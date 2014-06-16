#include "common/tcp_conn.h"
#include "common/common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

int tcp_conn_init(const char *host, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		log("socket");
		return -1;
	}

	struct sockaddr_in sa;
	struct addrinfo *ai = NULL;
	struct addrinfo hai;
	memset(&hai, 0, sizeof(hai));

	hai.ai_family = AF_INET;
	hai.ai_socktype = SOCK_STREAM;
	int addr_err = getaddrinfo(host, 0, &hai, &ai);
	if (addr_err != 0) {
		log("getaddrinfo: %s", gai_strerror(addr_err));
		goto err;
	}
	memcpy(&sa, ai->ai_addr, sizeof(sa));
	sa.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		log_errno("connect");
		goto err;
	}
	freeaddrinfo(ai);
	return fd;

 err:
	close(fd);
	freeaddrinfo(ai);
	return -1;
}
