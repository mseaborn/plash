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

#ifndef plash_shell_wait_h
#define plash_shell_wait_h


/* Registers a handler for when the given process exits.  The handler is
   also called when the process is stopped with a signal.  The handler
   is expected to free x when the process exits, but not when it is
   stopped. */
void w_add_process(int pid, void (*f)(void *x, int status), void *x);

void w_handle_process_status(int pid, int status);

void print_wait_status(FILE *fp, int status);

void w_setup();


#endif
