/* Copyright (C) 2004, 2005 Mark Seaborn

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

#ifndef filesysobj_real_h
#define filesysobj_real_h

#include "filesysobj.h"


/* Concrete types */

struct real_dir {
  struct filesys_obj hdr;
  struct stat stat;
  struct file_desc *fd; /* May be 0 if we failed to open the directory */
};

struct real_file {
  struct filesys_obj hdr;
  struct stat stat;
  struct file_desc *dir_fd;
  char *leaf;
};

struct real_symlink {
  struct filesys_obj hdr;
  struct stat stat; /* Result of lstat on symlink */
  struct file_desc *dir_fd;
  char *leaf;
};

DECLARE_VTABLE(real_dir_vtable);
DECLARE_VTABLE(real_file_vtable);
DECLARE_VTABLE(real_symlink_vtable);


struct filesys_obj *initial_dir(const char *pathname, int *err);


#endif
