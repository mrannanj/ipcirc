#pragma once

#include <stdint.h>

#define EV_NONE 0
#define EV_UNIX_ACCEPTED 1
#define EV_IRC_MESSAGE 2
#define EV_READY_TO_READ 3
#define EV_UNIX_MESSAGE 4
#define EV_COUNT 5

struct event {
  int type;
  uint32_t source;
  void *p;
};
