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

#include <unistd.h>
#include <stdio.h>

#include "region.h"
#include "serialise.h"


void arg_print_aux(FILE *fp, int indent, argmbuf_t buf, bufref_t x)
{
  {
    int i;
    if(!argm_int(buf, x, &i)) {
      fprintf(fp, "%i", i);
      return;
    }
  }
  {
    seqf_t str;
    if(!argm_str(buf, x, &str)) {
      fputc('"', fp);
      fprint_d(fp, str);
      fputc('"', fp);
      return;
    }
  }
  {
    cap_t c;
    if(!argm_cap(buf, x, &c)) {
      fprintf(fp, "cap");
      return;
    }
  }
  {
    int fd;
    if(!argm_fd(buf, x, &fd)) {
      fprintf(fp, "FD %i", fd);
      return;
    }
  }
  {
    int size;
    const bufref_t *arr;
    if(!argm_array(buf, x, &size, &arr)) {
      int i;
      fprintf(fp, "[");
      for(i = 0; i < size; i++) {
	arg_print_aux(fp, indent + 1, buf, arr[i]);
	if(i < size-1) {
	  int j;
	  fprintf(fp, ",\n");
	  for(j = 0; j < indent+1; j++) fputc(' ', fp);
	}
      }
      fprintf(fp, "]");
      return;
    }
  }
  fprintf(fp, "??");
}

void arg_print(FILE *fp, argmbuf_t buf, bufref_t x)
{
  arg_print_aux(fp, 0 /* indent */, buf, x);
  fputc('\n', fp);
}
