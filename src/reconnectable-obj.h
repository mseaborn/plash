/* Copyright (C) 2004 Mark Seaborn

   This file is part of Plash.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef reconnectable_obj_h
#define reconnectable_obj_h

#include "filesysobj.h"

struct reconnectable_obj {
  struct filesys_obj hdr;
  int head; /* True for the list head */
  cap_t x;
  struct reconnectable_obj *prev, *next;
};

extern struct reconnectable_obj reconnectable_list;

cap_t make_reconnectable(cap_t obj);

#endif
