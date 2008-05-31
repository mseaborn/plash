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

#ifndef filesysobj_union_h
#define filesysobj_union_h

struct filesys_obj *make_union_dir(struct filesys_obj *x, struct filesys_obj *y);

int merge_dir_lists(region_t r,
		    struct filesys_obj *dir1,
		    struct filesys_obj *dir2,
		    seqt_t *result,
		    int *err);

#endif
