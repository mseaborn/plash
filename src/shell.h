/* Copyright (C) 2004 Mark Seaborn

   This file is part of Plash, the Principle of Least Authority Shell.

   Plash is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Plash is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Plash; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#ifndef shell_h
#define shell_h

#include "region.h"
#include "parse-filename.h"


#define GLOB_STAR -1

#define REDIR_OUT_TRUNC		1
#define REDIR_OUT_APPEND	2
#define REDIR_IN		3
#define REDIR_IN_OUT		4

struct char_cons {
  int c; /* Usually a char; may also be GLOB_STAR */
  struct char_cons *next;
};
static inline struct char_cons *char_cons(region_t r, int c, struct char_cons *next)
{
  struct char_cons *n = region_alloc(r, sizeof(struct char_cons));
  n->c = c;
  n->next = next;
  return n;
}

seqf_t flatten_charlist(region_t r, struct char_cons *list);

struct str_list {
  char *str;
  struct str_list *next;
};


#endif
