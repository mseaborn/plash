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

#ifndef INC_PLASH_PYTHON_H
#define INC_PLASH_PYTHON_H


#include "filesysobj.h"

typedef struct {
  PyObject_HEAD;
  int fd;
} plpy_fd;

typedef struct {
  PyObject_HEAD;
  cap_t obj;
} plpy_obj;

struct plpy_pyobj {
  struct filesys_obj hdr;
  PyObject *obj;
};

extern PyTypeObject plpy_fd_type;
extern PyTypeObject plpy_obj_type;

extern PyTypeObject *plpy_wrapper_class;
extern PyTypeObject *plpy_pyobj_class;

extern PyObject *plpy_str_cap_invoke;
extern PyObject *plpy_str_cap_call;

DECLARE_VTABLE(plpy_pyobj_vtable);


void plpy_close(int fd);

PyObject *plpy_wrap_fd(int fd);

PyObject *plpy_obj_to_py(cap_t obj);
cap_t plpy_obj_from_py(PyObject *obj);

PyObject *plpy_cap_args_to_py(region_t r, struct cap_args args);
int plpy_cap_args_from_py(region_t r, PyObject *args, struct cap_args *out);

void plpy_init(void);


PyObject *plpy_wrap_fd_py(PyObject *self, PyObject *args);


#endif
