/* Copyright (C) 2006 Mark Seaborn

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#include "region.h"
#include "config-read.h"


/* Pathname that sandboxed processes should be told to use to invoke
   the dynamic linker, in order to handle execve(). */
const char *pl_ldso_path = "/special/ld-linux.so.2";

int string_has_prefix(const char *str1, const char *str2)
{
  while(1) {
    if(*str2 == 0) { return TRUE; }
    if(*str1 != *str2) { return FALSE; }
    str1++;
    str2++;
  }
}

void read_config_string(const char *str)
{
  if(string_has_prefix(str, "ldso-path=")) {
    pl_ldso_path = str + 10;
  }
}
