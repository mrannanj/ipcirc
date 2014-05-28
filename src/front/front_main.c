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

void die(const char* s) {
  perror(s);
  exit(1);
}

int main(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) die("socket");
  struct sockaddr_un sa = {
    .sun_family = AF_UNIX,
    .sun_path = "konventio.sock"
  };
  if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) die("connect");
  printf("connected\n");
}
