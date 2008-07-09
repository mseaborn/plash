/* Copyright (C) 2007, 2008 Mark Seaborn

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

#ifndef LIB_POWERBOX_H
#define LIB_POWERBOX_H


#include <glib.h>

struct pb_result;

gboolean pb_is_available(void);

gboolean pb_request_send(gboolean want_save, gboolean want_directory,
			 int parent_window_id,
			 void (*callback)(void *handle,
					  struct pb_result *result),
			 void *handle);

gboolean pb_result_was_cancelled(struct pb_result *result);
char *pb_result_get_filename(struct pb_result *result);
void pb_result_free(struct pb_result *result);


#endif
