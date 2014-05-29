#pragma once

#include <stddef.h>

#define UNIX_PATH_MAX 108
#define UNIX_ADDR_PREFIX "@ipc-irc.sock"

void die(const char*);
void die2(const char*);
int find_server_addr(char*, size_t*);
