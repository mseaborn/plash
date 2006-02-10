/* Copyright (C) 2004, 2005 Mark Seaborn

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

#ifndef marshal_exec_h
#define marshal_exec_h


#include "serialise.h"

struct fd_mapping {
  int fd_no;
  int fd;
};

struct exec_args {
  char **argv, **env;
  struct fd_mapping *fds;
  int fds_count;
  cap_t root_dir;
  seqf_t cwd;
  int got_cwd;
  int pgid;
};

int unpack_exec_args(region_t r, struct arg_m_buf argbuf, bufref_t args_ref,
		     struct exec_args *ea);


#endif
