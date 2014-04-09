#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

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

int main(int argc, char *argv[]) {
  short port = 6667;
  char *host = "irc.elisa.fi";

  char *nick = "berberiTEST";
  char *user = "berberi";
  socket_handle = my_connect_sock(host, port);
  send_data("NICK %s", nick);
  send_data("USER %s 8 * :%s", user, user);
  send_data("MODE %s +i", nick);
  while(1) {
    send_action();
    read_data();
  }

  close(socket_handle);

  printf("lol");

  return 0;
}
