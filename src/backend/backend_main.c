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

#if 0
char output_buffer[2048], *output = output_buffer;
int socket_handle;

int my_connect_sock(char *host, short port) {
  int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_address;

  if (descriptor < 0)
    exit(1);

  struct addrinfo *ai, hai = { 0 };
  hai.ai_family = AF_INET;
  hai.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, 0, &hai, &ai))
    exit(1);
  memcpy(&server_address, ai->ai_addr, sizeof(server_address));
  server_address.sin_port = htons(port);
  freeaddrinfo(ai);

  if (connect(descriptor, (struct sockaddr *) &server_address,
    sizeof(server_address)) < 0)
    exit(1);

  return descriptor;
}

void send_data(const char *message, ...) {
  va_list vl;
  size_t n, l = 2048-(output-output_buffer);

  if (l<2) return;
  va_start(vl, message);
  n=vsnprintf(output, l-2, message, vl);
  va_end(vl);
  output += n>l-2 ? l-2 : n;
  // IRC end characters:
  *output++ = '\r';
  *output++ = '\n';
}

int read_data(void) {
  char buffer[2048];
  int read_status;

  read_status = read(socket_handle, buffer, 2048);
  if (read_status < 0) {
    fprintf(stderr, "IO error while reading.");
    exit(1);
  }
  if (!read_status) return 0; // No Messages
  printf("%s", buffer);
  bzero(buffer, 2048);
  return read_status;
}

void send_action() {
  int write_status = write(socket_handle, output_buffer, output-output_buffer);
  if (write_status < 0) {
    fprintf(stderr, "Write error.");
    exit(1);
  }
  if (!write_status) return;
  output -= write_status;
  memmove(output_buffer, output_buffer + write_status, output-output_buffer);
}

int main(void) {
  short port = 6667;
  char host[] = "irc.cs.hut.fi";
  char nick[] = "berberiTEST";
  char user[] = "berberi";

  socket_handle = my_connect_sock(host, port);
  send_data("NICK %s", nick);
  send_data("USER %s 8 * :%s", user, user);
  send_data("MODE %s +i", nick);
  send_data("JOIN #udpgame");

  while(1) {
    send_action();
    read_data();
  }

  close(socket_handle);

  return 0;
}
#endif

void die(const char* s) {
  perror(s);
  exit(1);
}

#define LISTEN_BACKLOG 10
#define MAX_EVENTS 10

int main(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) die("socket");
  struct sockaddr_un sa = {
    .sun_family = AF_UNIX,
    .sun_path = "konventio.sock"
  };
  if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) die("bind");
  if (listen(fd, LISTEN_BACKLOG) < 0) die("listen");

  struct sockaddr_un client;
  socklen_t addrlen = sizeof(client);
  int epfd = epoll_create1(0);
  if (epfd < 0) die("epoll_create1");
  struct epoll_event event = { .events = EPOLLIN, .data.fd = fd };
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) < 0) die("epoll_ctl");
  struct epoll_event es[MAX_EVENTS];

  for (;;) {
    int nfd = epoll_wait(epfd, es, MAX_EVENTS, -1);
    if (nfd < 0) die("epoll_wait");
    for (int i = 0; i < nfd; ++i) {
      int efd = es[i].data.fd;
      if (efd == fd) {
        int acc = accept(fd, (struct sockaddr*)&client, &addrlen);
        if (acc < 0) die("accept");
        struct epoll_event ev = { .events = EPOLLIN, .data.fd = acc };
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, acc, &ev) < 0) die("epoll_ctl");
        printf("accepted %d\n", acc);
      } else {
        printf("socket %d is ready\n", efd);
        exit(0);
      }
    }
  }
}
