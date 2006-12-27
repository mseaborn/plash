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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

/* For RTLD_NEXT */
#define _GNU_SOURCE

#include <dlfcn.h>


/* This is used in the LD_PRELOADable build of Plash's libc code,
   which is used only for testing purposes.

   The shared object needs to be able to both import and export
   symbols of the same name.  e.g. It wraps the close() function.  We
   need to look up symbols at run time using dlsym() in order to do
   this.

   We use gcc's __builtin_apply() facility so that:
   1) We can make the imported functions appear as functions not
   function pointers.  This means we don't have to rebuild Plash's
   libc code for linking into preload-libc.so.
   2) We don't have to specify the functions' prototypes.

   __builtin_apply() might disappear from gcc in the future.  May have
   to use libffi instead in that case. */


#define IMPORT(name) \
void name() \
{ \
  void *sym = dlsym(RTLD_NEXT, #name); \
  /* 100 is the maximum size of the arguments on the stack. */ \
  __builtin_return(__builtin_apply(sym, __builtin_apply_args(), 100)); \
}

IMPORT(__fxstat);
IMPORT(close);
IMPORT(dup);
IMPORT(dup2);
IMPORT(fork);
IMPORT(execve);
IMPORT(bind);
IMPORT(connect);
IMPORT(getsockname);
IMPORT(getsockopt);
IMPORT(getuid);
IMPORT(getgid);
