/* Copyright (C) 2007 Mark Seaborn

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


#define t_check_zero(expr) t_check_zero_func(expr, #expr)
#define t_check(expr) t_check_func(expr, #expr)

void t_check_zero_func(int return_code, const char *expr);
void t_check_func(int x, const char *expr);
int get_dir_fd(const char *dir_pathname);
