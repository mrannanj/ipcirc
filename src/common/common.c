#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"

#define PROC_NET_UNIX "/proc/net/unix"

void log_(const char *fn, const char *fun, int ln, ...)
{
	va_list args;
	char buf[1024];
	char *fmt;
	int n = snprintf(buf, sizeof(buf), "%s:%d:%s: ", fn, ln, fun);

	if (n < 0)
		die2("snprintf error");

	va_start(args, ln);
	fmt = va_arg(args, char *);
	vsnprintf(&buf[n], sizeof(buf) - n, fmt, args);
	va_end(args);

	fprintf(stderr, "%s\n", buf);
}

void log_errno_(const char *s, const char *fn, int ln)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%s:%d:%s", fn, ln, s);
	perror(buf);
}

void die_(const char *s, const char *fn, int ln)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%s:%d:%s", fn, ln, s);
	perror(buf);
	exit(EXIT_FAILURE);
}

void die2_(const char *msg, const char *fn, const char *fun, int ln)
{
	fprintf(stderr, "%s:%d:%s: %s\n", fn, ln, fun, msg);
	exit(EXIT_FAILURE);
}

int find_server_addr(char *path, size_t * len)
{
	char line[512];
	int found = 0;
	FILE *f = fopen(PROC_NET_UNIX, "r");

	if (!f)
		die("fopen");

	fgets(line, sizeof(line), f);
	while (fgets(line, sizeof(line), f)) {
		unsigned long int uli;
		char *s;

		sscanf(line, "%lX: %lX %lX %lX %lX %lX %lX %s",
		       &uli, &uli, &uli, &uli, &uli, &uli, &uli, path);
		s = strstr(path, UNIX_ADDR_PREFIX);
		if (s) {
			found = 1;
			*len = strlen(path);
			break;
		}
	}
	fclose(f);

	return found;
}

int min(int a, int b)
{
	return a < b ? a : b;
}

int max(int a, int b)
{
	return a > b ? a : b;
}
