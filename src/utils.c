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

/* For asprintf() */
#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include "region.h"


void print_data(seqf_t b)
{
  fprint_data(stdout, b);
}

void fprint_data(FILE *fp, seqf_t b)
{
  fprintf(fp, "data of length %i:\n", b.size);
  while(b.size > 0) {
    int i = 0;
    fputc(' ', fp); fputc(' ', fp);
    if(b.size >= 4) {
      fprintf(fp, "[%i]", *(int *) b.data);
    }
    fputc('"', fp);
    while(i < 4 && i < b.size) {
      char c = b.data[i];
      if(c < 32) { fprintf(fp, "\\%03i", (unsigned char) c); }
      else { fputc(c, fp); }
      i++;
    }
    fputc('"', fp);
    fputc('\n', fp);
    b.data += i;
    b.size -= i;
  }
}

void fprint_d(FILE *fp, seqf_t b)
{
  fwrite(b.data, b.size, 1, fp);
}

seqt_t mk_printf(region_t r, const char *fmt, ...)
{
  va_list args;
  char *buf, *str;
  int got;

  va_start(args, fmt);
  got = vasprintf(&buf, fmt, args);
  assert(got >= 0);
  va_end(args);

  str = region_alloc(r, got);
  memcpy(str, buf, got);
  free(buf);

  return mk_leaf2(r, str, got);
}
