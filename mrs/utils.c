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
  char buf[256], *x;
  va_list args;
  int got;

  va_start(args, fmt);
  got = vsnprintf(buf, sizeof(buf), fmt, args);
  x = region_alloc(r, got);
  memcpy(x, buf, got);
  return mk_leaf2(r, x, got);
}
