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

#include <Python.h>

#include "filesysobj.h"
#include "cap-utils.h"
#include "marshal.h"

#include "py-plash.h"

PyObject *plpy_str_cap_invoke;
PyObject *plpy_str_cap_call;


PyTypeObject *plpy_wrapper_class = NULL;
PyTypeObject *plpy_pyobj_class = NULL;

void plpy_init(void)
{
  PyObject *bases = NULL, *dict = NULL, *name = NULL;
  
  plpy_str_cap_invoke = PyString_InternFromString("cap_invoke");
  if(!plpy_str_cap_invoke) { goto error; }
  plpy_str_cap_call = PyString_InternFromString("cap_call");
  if(!plpy_str_cap_call) { goto error; }

  bases = Py_BuildValue("(O)", (PyObject *) &plpy_obj_type);
  if(!bases) { goto error; }
  dict = PyDict_New();
  if(!dict) { goto error; }
  name = Py_BuildValue("s", "plash_object");
  if(!name) { goto error; }
  
  plpy_wrapper_class = (PyTypeObject *) PyClass_New(bases, dict, name);
  if(!PyType_Check(plpy_wrapper_class)) {
    assert(0);
  }

 error:
  Py_XDECREF(bases);
  Py_XDECREF(dict);
  Py_XDECREF(name);

  {
    PyObject *dict = NULL, *name = NULL;
  
    dict = PyDict_New();
    if(!dict) { goto error2; }
    name = Py_BuildValue("s", "pyobj");
    if(!name) { goto error2; }
  
    plpy_pyobj_class = (PyTypeObject *) PyClass_New(NULL, dict, name);
    if(!PyType_Check(plpy_pyobj_class)) {
      assert(0);
    }

  error2:
    Py_XDECREF(dict);
    Py_XDECREF(name);
  }
}

/* Takes an owning reference to a C Plash object, and returns a Python
   Plash object (owning reference). */
PyObject *plpy_obj_to_py(cap_t obj)
{
  if(obj->vtable == &plpy_pyobj_vtable) {
    /* Return the Python object being wrapped. */
    struct plpy_pyobj *wrapper = (void *) obj;
    Py_INCREF(wrapper->obj);
    return wrapper->obj;
  }
  else {
    plpy_obj *wrapper =
      (plpy_obj *) plpy_wrapper_class->tp_alloc(plpy_wrapper_class, 0);
    if(!wrapper) {
      filesys_obj_free(obj);
      return NULL;
    }
    wrapper->obj = obj;
    return (PyObject *) wrapper;
  }
}

/* Takes a borrowed reference to a Python object, and returns a C
   Plash object (owning reference).  Returns NULL (error) if the
   conversion cannot be done. */
cap_t plpy_obj_from_py(PyObject *obj)
{
  if(PyObject_IsInstance(obj, (PyObject *) plpy_pyobj_class)) {
    struct plpy_pyobj *wrapper =
      filesys_obj_make(sizeof(struct plpy_pyobj), &plpy_pyobj_vtable);
    Py_INCREF(obj);
    wrapper->obj = obj;
    return (cap_t) wrapper;
  }
  else if(PyObject_TypeCheck(obj, &plpy_obj_type)) {
    return inc_ref(((plpy_obj *) obj)->obj);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "arg is not a Plash object");
    return NULL;
  }
}


static void plpy_obj_dealloc(plpy_obj *self)
{
  filesys_obj_free(self->obj);
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *plpy_obj_repr(plpy_obj *self)
{
  return PyString_FromFormat("<plash object %s %p, refcount=%i>",
			     self->obj->vtable->vtable_name, self->obj,
			     self->obj->refcount);
}

/*
static PyObject *plpy_compare(plpy_obj *obj1, PyObject *obj2)
{
  ...
}
*/

static int plpy_obj_hash(plpy_obj *self)
{
  return (int) self->obj;
}


/* Takes "args" as an owned reference.  Converts it to a Python version.
   The region r is only used to flatten the data argument. */
PyObject *plpy_cap_args_to_py(region_t r, struct cap_args args)
{
  int i;
  PyObject *caps = NULL, *fds = NULL, *result;
  seqf_t data = flatten_reuse(r, args.data);
  
  /* Wrap capability arguments. */
  caps = PyTuple_New(args.caps.size);
  if(!caps) { goto error; }
  for(i = 0; i < args.caps.size; i++) {
    PyObject *wrapper = plpy_obj_to_py(inc_ref(args.caps.caps[i]));
    if(!wrapper) { goto error; }
    PyTuple_SET_ITEM(caps, i, wrapper);
  }

  /* Wrap file descriptor arguments. */
  fds = PyTuple_New(args.fds.count);
  if(!fds) { goto error; }
  for(i = 0; i < args.fds.count; i++) {
    PyObject *wrapper = plpy_wrap_fd(args.fds.fds[i]);
    if(!wrapper) {
      /* Error: free the remaining FDs. */
      i++;
      for(; i < args.fds.count; i++) { plpy_close(args.fds.fds[i]); }
      goto error_fds_done;
    }
    PyTuple_SET_ITEM(fds, i, wrapper);
  }
  
  result = Py_BuildValue("(s#OO)", data.data, data.size, caps, fds);
  caps_free(args.caps);
  Py_DECREF(caps);
  Py_DECREF(fds);
  return result;

 error:
  close_fds(args.fds);
 error_fds_done:
  caps_free(args.caps);
  Py_XDECREF(caps);
  Py_XDECREF(fds);
  return NULL;
}

int plpy_cap_args_from_py(region_t r, PyObject *args, struct cap_args *out)
{
  char *data;
  int data_size;
  PyObject *caps, *fds;
  cap_t *caps2;
  int *fds2;
  int i;

  if(!PyArg_ParseTuple(args, "s#OO", &data, &data_size, &caps, &fds) ||
     !PyTuple_Check(caps) ||
     !PyTuple_Check(fds)) {
    PyErr_SetString(PyExc_TypeError, "Plash arguments tuple not valid");
    return FALSE;
  }
  
  /* Data portion is borrowed from the Python string. */
  out->data = mk_leaf2(r, data, data_size);

  /* Copy object references. */
  out->caps.size = PyTuple_GET_SIZE(caps);
  caps2 = region_alloc(r, out->caps.size * sizeof(cap_t));
  out->caps.caps = caps2;
  for(i = 0; i < out->caps.size; i++) {
    PyObject *obj = PyTuple_GET_ITEM(caps, i);
    cap_t obj2 = plpy_obj_from_py(obj);
    if(!obj2) {
      /* If an error occurs, free the references that we've copied so far. */
      int j;
      for(j = 0; j < i; j++) { filesys_obj_free(caps2[j]); }
      return FALSE;
    }
    caps2[i] = obj2;
  }

  /* Copy file descriptors. */
  out->fds.count = PyTuple_GET_SIZE(fds);
  fds2 = region_alloc(r, out->fds.count * sizeof(int));
  out->fds.fds = fds2;
  for(i = 0; i < out->fds.count; i++) {
    int fd_copy;
    PyObject *fd = PyTuple_GET_ITEM(fds, i);
    if(!PyObject_TypeCheck(fd, &plpy_fd_type)) {
      /* Error */
      int j;
      for(j = 0; j < i; j++) { plpy_close(fds2[j]); }
      caps_free(out->caps);
      PyErr_SetString(PyExc_TypeError, "arg is not a wrapped FD");
      return FALSE;
    }
    fd_copy = dup(((plpy_fd *) fd)->fd);
    if(fd_copy < 0) {
      /* Error */
      int j;
      for(j = 0; j < i; j++) { plpy_close(fds2[j]); }
      caps_free(out->caps);
      PyErr_SetFromErrno(PyExc_Exception);
      return FALSE;
    }
    fds2[i] = fd_copy;
  }
  
  return TRUE;
}

static PyObject *plpy_obj_cap_invoke(plpy_obj *self, PyObject *args)
{
  region_t r = region_make();
  struct cap_args args2;
  
  if(!plpy_cap_args_from_py(r, args, &args2)) {
    region_free(r);
    return NULL;
  }
  cap_invoke(self->obj, args2);
  region_free(r);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *plpy_obj_cap_call(plpy_obj *self, PyObject *args)
{
  region_t r = region_make();
  struct cap_args args2, result;
  PyObject *result2;
  
  if(!plpy_cap_args_from_py(r, args, &args2)) {
    region_free(r);
    return NULL;
  }
  cap_call(self->obj, r, args2, &result);
  
  result2 = plpy_cap_args_to_py(r, result);
  region_free(r);
  return result2;
}

static PyMethodDef plpy_obj_object_methods[] = {
  { "cap_call", (PyCFunction) plpy_obj_cap_call, METH_O,
    "Invoke Plash object, getting a result back."
  },
  { "cap_invoke", (PyCFunction) plpy_obj_cap_invoke, METH_O,
    "Send a message to a Plash object, without getting a result back directly."
  },
  {NULL}  /* Sentinel */
};

PyTypeObject plpy_obj_type = {
  PyObject_HEAD_INIT(NULL)
  0,                         /* ob_size */
  "plash.Plash",             /* tp_name */
  sizeof(plpy_obj),          /* tp_basicsize */
  0,                         /* tp_itemsize */
  (destructor) plpy_obj_dealloc, /* tp_dealloc */
  0,                         /* tp_print */
  0,                         /* tp_getattr */
  0,                         /* tp_setattr */
  0,                         /* tp_compare */
  (reprfunc) plpy_obj_repr,  /* tp_repr */
  0,                         /* tp_as_number */
  0,                         /* tp_as_sequence */
  0,                         /* tp_as_mapping */
  (hashfunc) plpy_obj_hash,  /* tp_hash  */
  0,                         /* tp_call */
  0,                         /* tp_str */
  0,                         /* tp_getattro */
  0,                         /* tp_setattro */
  0,                         /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
  "Plash objects",           /* tp_doc */
  0,                         /* tp_traverse */
  0,                         /* tp_clear */
  0,                         /* tp_richcompare */
  0,                         /* tp_weaklistoffset */
  0,                         /* tp_iter */
  0,                         /* tp_iternext */
  plpy_obj_object_methods,   /* tp_methods */
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


void plpy_pyobj_free(struct filesys_obj *obj1)
{
  struct plpy_pyobj *obj = (void *) obj1;
  Py_DECREF(obj->obj);
}

void plpy_pyobj_cap_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct plpy_pyobj *obj = (void *) obj1;
  PyObject *result;
  region_t r = region_make();
  PyObject *args2 = plpy_cap_args_to_py(r, args);
  region_free(r);
  if(!args2) {
    /* Error: but must be ignored. */
    return;
  }
  result = PyObject_CallMethodObjArgs(obj->obj, plpy_str_cap_invoke,
				      args2, NULL);
  if(!result) {
    /* Any error is ignored. */
    PyErr_Clear();
  }
  Py_DECREF(args2);
}

void plpy_pyobj_cap_call(struct filesys_obj *obj1, region_t r,
			 struct cap_args args, struct cap_args *result2)
{
  struct plpy_pyobj *obj = (void *) obj1;
  PyObject *result;
  PyObject *args2 = plpy_cap_args_to_py(r, args);
  if(!args2) {
    /* Error */
    *result2 = cap_args_d(mk_int(r, METHOD_FAIL));
    return;
  }
  result = PyObject_CallMethodObjArgs(obj->obj, plpy_str_cap_call,
				      args2, NULL);
  if(!result) {
    /* Any error is ignored. */
    PyErr_Clear();
    *result2 = cap_args_d(mk_int(r, METHOD_FAIL));
  }
  else {
    if(!plpy_cap_args_from_py(r, result, result2)) {
      *result2 = cap_args_d(mk_int(r, METHOD_FAIL));
    }
  }
  Py_DECREF(args2);
}

#include "out-vtable-python.h"
