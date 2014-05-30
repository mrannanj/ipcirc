#pragma once

#include <stddef.h>

#define UNIX_PATH_MAX 108
#define UNIX_ADDR_PREFIX "@ipc-irc.sock"

void log_(const char*, const char*, int, ...);
void log_errno_(const char*, const char*, int);

#define log(...) log_(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define log_errno(s) log_errno_((s), __FILE__, __LINE__)

void die_(const char*, const char*, int);
void die2_(const char*, const char*, const char*, int);

#define die(s) die_((s), __FILE__, __LINE__)
#define die2(s) die2_((s), __FILE__, __func__ ,__LINE__)

int find_server_addr(char*, size_t*);


