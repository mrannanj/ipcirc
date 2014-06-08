#pragma once

#include <stdint.h>
#include <unistd.h>

struct event {
  int type;
  struct conn* source;
  void *p;
};
