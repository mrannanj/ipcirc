#pragma once

#include <stdint.h>
#include <unistd.h>

struct event {
  int type;
  uint32_t source;
  void *p;
};
