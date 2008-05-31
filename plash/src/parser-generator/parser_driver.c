/* Copyright (C) 2004 Mark Seaborn

   This file is part of Parser Generator.

   Parser Generator is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Parser Generator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Parser Generator; see the file COPYING.  If not, write
   to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA.  */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>


int f_defs(const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);

int main()
{
  const char *filename = "gram.gram";
  int fd;
  struct stat st;
  int size;
  char *buf;

  fd = open(filename, O_RDONLY);
  if(fd < 0) { perror(filename); return 1; }
  if(fstat(fd, &st) < 0) { perror("stat"); return 1; }
  size = st.st_size;

  buf = malloc(size);
  assert(buf);
  {
    int got = 0;
    while(got < st.st_size) {
      int x = read(fd, buf + got, size - got);
      if(x < 0) { perror("read"); return 1; }
      assert(x > 0);
      got += x;
    }
  }
  {
    const char *pos_out;
    void *val_out;
    if(f_defs(buf, buf + size, &pos_out, &val_out))
      printf("parsed ok upto %i\n", pos_out - buf);
    else printf("parse failed\n");
  }
  return 0;
}
