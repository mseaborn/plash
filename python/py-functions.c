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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#include <unistd.h>
#include <fcntl.h>

#include <Python.h>

#include "filesysobj.h"
#include "filesysobj-real.h"
#include "cap-protocol.h"
#include "fs-operations.h"

#include "py-plash.h"


static PyObject *plpy_initial_dir(PyObject *self, PyObject *args)
{
  char *pathname;
  cap_t dir;
  int err;
  if(!PyArg_ParseTuple(args, "s", &pathname)) { return NULL; }
  dir = initial_dir(pathname, &err);
  if(dir) {
    return plpy_obj_to_py(dir);
  }
  else {
    errno = err;
    return PyErr_SetFromErrnoWithFilename(PyExc_Exception, pathname);
  }
}

static PyObject *plpy_make_fs_op(PyObject *self, PyObject *args)
{
  plpy_obj *root_dir;
  struct server_shared *shared;
  if(!PyArg_ParseTuple(args, "O!", &plpy_obj_type, &root_dir)) { return NULL; }
  
  shared = amalloc(sizeof(struct server_shared));
  shared->refcount = 1;
  shared->next_id = 1;
  {
    int fd = dup(STDERR_FILENO);
    if(fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
      return PyErr_SetFromErrno(PyExc_Exception);
    }
    shared->log = fdopen(fd, "w");
    setvbuf(shared->log, 0, _IONBF, 0);
  }
  shared->log_summary = 0;
  shared->log_messages = 0;
  shared->call_count = 0;
  return plpy_obj_to_py(make_fs_op_server(shared, inc_ref(root_dir->obj),
					  NULL /* cwd */));
}

static PyObject *plpy_make_conn_maker(PyObject *self, PyObject *args)
{
  return plpy_obj_to_py(conn_maker_make());
}

static PyObject *plpy_run_server(PyObject *self, PyObject *args)
{
  cap_run_server();
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef module_methods[] = {
  { "initial_dir", plpy_initial_dir, METH_VARARGS,
    "Get an initial real_dir object." },
  { "make_fs_op", plpy_make_fs_op, METH_VARARGS,
    "Get an initial real_dir object." },
  { "make_conn_maker", plpy_make_conn_maker, METH_NOARGS,
    "Returns an object that creates connections." },
  { "run_server", plpy_run_server, METH_NOARGS,
    "Enter event loop, handling incoming object invocations as a server." },
  { NULL, NULL, 0, NULL }  /* Sentinel */
};


void initplash(void)
{
  PyObject *mod;

  if(PyType_Ready(&plpy_fd_type) < 0) { return; }
  if(PyType_Ready(&plpy_obj_type) < 0) { return; }

  mod = Py_InitModule3("plash", module_methods,
		       "Plash module.");

  Py_INCREF(&plpy_fd_type);
  PyModule_AddObject(mod, "FD", (PyObject *) &plpy_fd_type);
  Py_INCREF(&plpy_obj_type);
  PyModule_AddObject(mod, "Plash", (PyObject *) &plpy_obj_type);
}
