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

#include <errno.h>
#include <sysdep.h>
#include <not-cancel.h>


/* Uncancelable open.  */

int open_not_cancel(const char *name, int flags, int mode)
{
  return INLINE_SYSCALL (open, 3, (const char *) (name), (flags), (mode));
}

int open_not_cancel_2(const char *name, int flags)
{
  return INLINE_SYSCALL (open, 2, (const char *) (name), (flags));
}
