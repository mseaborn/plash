
#ifndef shell_h
#define shell_h

#include "region.h"


struct char_cons {
  char c;
  struct char_cons *next;
};
static inline struct char_cons *char_cons(region_t r, char c, struct char_cons *next)
{
  struct char_cons *n = region_alloc(r, sizeof(struct char_cons));
  n->c = c;
  n->next = next;
  return n;
}


#endif
