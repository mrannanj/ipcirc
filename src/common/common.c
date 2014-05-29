#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#define PROC_NET_UNIX "/proc/net/unix"

void die(const char* s) {
  perror(s);
  exit(EXIT_FAILURE);
}

void die2(const char* s) {
  fprintf(stderr, "%s", s);
  exit(EXIT_FAILURE);
}

int find_server_addr(char* path, size_t* len) {
  FILE *f = fopen(PROC_NET_UNIX, "r");
  if (!f) die("fopen");
  char line[512];
  int found = 0;
  fgets(line, sizeof(line), f);
  while (fgets(line, sizeof(line), f)) {
    unsigned long uli;
    sscanf(line, "%lX: %lX %lX %lX %lX %lX %lX %s",
        &uli, &uli, &uli, &uli, &uli, &uli, &uli, path);
    char* s = strstr(path, UNIX_ADDR_PREFIX);
    if (s) {
      found = 1;
      *len = strlen(path);
      break;
    }
  }
  fclose(f);
  return found;
}
