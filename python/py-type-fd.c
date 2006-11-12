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

#include <unistd.h>

#include <Python.h>

#include "py-plash.h"


/* Wrapper for file descriptors.  The function this performs is to
   close the file descriptor when the wrapper is freed, which could be
   done in Python code.  In the future, Plash could have a refcounted
   FD wrapper at the C level, at which point this Python type could
   wrap that instead. */


/* This could warn when close() returns an error. */
void plpy_close(int fd)
{
  if(close(fd) < 0) {
    /* perror("close"); */
  }
}

PyObject *plpy_wrap_fd(int fd)
{
  plpy_fd *wrapper = (plpy_fd *) plpy_fd_type.tp_alloc(&plpy_fd_type, 0);
  if(!wrapper) {
    plpy_close(fd);
    return NULL;
  }
  wrapper->fd = fd;
  return (PyObject *) wrapper;
}


static void plpy_fd_dealloc(plpy_fd *self)
{
  plpy_close(self->fd);
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *plpy_fd_repr(plpy_fd *self)
{
  return PyString_FromFormat("<fd %i>", self->fd);
}


/* fileno method: returns file descriptor number, borrowed from wrapper. */
static PyObject *plpy_fd_fileno(plpy_fd *self, PyObject *args)
{
  return Py_BuildValue("i", self->fd);
}

static PyMethodDef plpy_fd_object_methods[] = {
  { "fileno", (PyCFunction) plpy_fd_fileno, METH_NOARGS,
    "Returns file descriptor number."
  },
  {NULL}  /* Sentinel */
};


PyTypeObject plpy_fd_type = {
  PyObject_HEAD_INIT(NULL)
  0,                         /* ob_size */
  "plash_core.FD",           /* tp_name */
  sizeof(plpy_fd),           /* tp_basicsize */
  0,                         /* tp_itemsize */
  (destructor) plpy_fd_dealloc,   /* tp_dealloc */
  0,                         /* tp_print */
  0,                         /* tp_getattr */
  0,                         /* tp_setattr */
  0,                         /* tp_compare */
  (reprfunc) plpy_fd_repr,   /* tp_repr */
  0,                         /* tp_as_number */
  0,                         /* tp_as_sequence */
  0,                         /* tp_as_mapping */
  0,                         /* tp_hash  */
  0,                         /* tp_call */
  0,                         /* tp_str */
  0,                         /* tp_getattro */
  0,                         /* tp_setattro */
  0,                         /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
  "Wrapper for file descriptors",           /* tp_doc */
  0,                         /* tp_traverse */
  0,                         /* tp_clear */
  0,                         /* tp_richcompare */
  0,                         /* tp_weaklistoffset */
  0,                         /* tp_iter */
  0,                         /* tp_iternext */
  plpy_fd_object_methods,    /* tp_methods */
  0,                         /* tp_members */
  0,                         /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  0                          /* tp_new */
};
