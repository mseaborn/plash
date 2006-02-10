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


struct iovec;

int open_not_cancel(const char *name, int flags, int mode);
int open_not_cancel_2(const char *name, int flags);
int close_not_cancel(int fd);
void close_not_cancel_no_status(int fd);
int read_not_cancel(int fd, void *buf, int n);
int write_not_cancel(int fd, const void *buf, int n);
void writev_not_cancel_no_status(int fd, const struct iovec *iov, int n);
int fcntl_not_cancel(int fd, int cmd, long val);
int waitpid_not_cancel(int pid, int *stat_loc, int options);
