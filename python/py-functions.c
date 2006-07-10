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
#include "marshal.h"
#include "marshal-pack.h"
#include "build-fs.h"
#include "plash-libc.h"

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

static PyObject *plpy_cap_close_all_connections(PyObject *self, PyObject *args)
{
  cap_close_all_connections();
  Py_INCREF(Py_None);
  return Py_None;
}

__asm__(".weak plash_libc_duplicate_connection");
__asm__(".weak plash_libc_kernel_execve");

static PyObject *plpy_libc_duplicate_connection(PyObject *self, PyObject *args)
{
  if(plash_libc_duplicate_connection) {
    int fd = plash_libc_duplicate_connection();
    if(fd < 0) {
      PyErr_SetString(PyExc_Exception,
		      "plash_libc_duplicate_connection() failed");
      return NULL;
    }
    else {
      return plpy_wrap_fd(fd);
    }
  }
  else {
    PyErr_SetString(PyExc_Exception,
		    "The symbol plash_libc_duplicate_connection is not defined; the process is probably not running under the Plash environment");
    return NULL;
  }
}

/* This calls the execve() system call, without being intercepted by
   Plash's changes to libc.  The fact that we have to duplicate
   Python's wrapping of execve() is a bit of a pain.  Maybe Plash's
   libc should provide a flag to disable its interception instead,
   rather than providing an alternative entry point. */
/* This is somewhat simpler than Python's execve() wrapper in
   posixmodule.c because it doesn't perform any conversions, which
   should arguably be done in Pythonland instead of C. */
static PyObject *plpy_libc_kernel_execve(PyObject *self, PyObject *args)
{
  char *path;
  PyObject *argv, *env;
  char **argv_list, **env_list;
  int argv_size;
  int env_size;
  int i;
  if(!PyArg_ParseTuple(args, "sOO:kernel_execve",
		       &path, &argv, &env) ||
     !PyTuple_Check(argv) ||
     !PyTuple_Check(env)) {
    return NULL;
  }

  if(!plash_libc_kernel_execve) {
    PyErr_SetString(PyExc_Exception,
		    "The symbol plash_libc_kernel_execve is not defined; the process is probably not running under the Plash environment");
    return NULL;
  }
  
  argv_size = PyTuple_Size(argv);
  argv_list = PyMem_NEW(char *, argv_size + 1);
  if(!argv_list) {
    PyErr_NoMemory();
    goto fail_0;
  }
  for(i = 0; i < argv_size; i++) {
    if(!PyArg_Parse(PyTuple_GetItem(argv, i), "s", &argv_list[i])) {
      goto fail_1;
    }
  }
  argv_list[argv_size] = NULL;
  
  env_size = PyTuple_Size(env);
  env_list = PyMem_NEW(char *, env_size + 1);
  if(!env_list) {
    PyErr_NoMemory();
    goto fail_1;
  }
  for(i = 0; i < env_size; i++) {
    if(!PyArg_Parse(PyTuple_GetItem(env, i), "s", &env_list[i])) {
      goto fail_2;
    }
  }
  env_list[env_size] = NULL;

  plash_libc_kernel_execve(path, argv_list, env_list);

  PyErr_SetFromErrno(PyExc_OSError);
 fail_2:
  PyMem_Free(env_list);
 fail_1:
  PyMem_Free(argv_list);
 fail_0:
  return NULL;
}


/* Note: This should properly be made into a case in marshal_cap_call().
   That would make this method available on the regular conn_maker object,
   instead of requiring a separate object.
   However, I'm not convinced it's safe for this method to be exported
   to other processes.  It means the server process would accept
   connections involving FDs created by other processes.  If the FDs
   aren't socket FDs, it might do something unexpected. */
static void plpy_make_conn2(cap_t obj, region_t r, struct cap_args args,
			    struct cap_args *result)
{
  int fd;
  int import_count;
  cap_seq_t export_caps;
  if(pl_unpack(r, args, METHOD_MAKE_CONN2, "fiC",
	       &fd, &import_count, &export_caps)) {
    cap_t *import;
#if 1
    import = cap_make_connection(r, fd, export_caps, import_count,
				 "to-client");
    *result = pl_pack(r, METHOD_R_MAKE_CONN2, "C",
		      cap_seq_make(import, import_count));
#else
    int rc = obj->vtable->make_conn2(obj, r, args.fds.fds[0], args.caps,
				     import_count, &import);
    caps_free(args.caps);
    if(rc < 0) {
      *result = pl_pack(r, METHOD_FAIL, "i", 0); /* FIXME: fill out error */
    }
    else {
      *result = pl_pack(r, METHOD_R_MAKE_CONN2, "C",
			cap_seq_make(import, import_count));
    }
#endif
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
    pl_args_free(&args);
  }
}

static void plpy_resolve_dir(cap_t obj, region_t r, struct cap_args args,
			     struct cap_args *result)
{
  cap_t root, cwd;
  int symlink_limit;
  seqf_t pathname;
  if(pl_unpack(r, args, METHOD_RESOLVE_DIR, "cdiS",
	       &root, &cwd, &symlink_limit, &pathname) &&
     (cwd == NULL || dir_stack_upcast(cwd) != NULL))
  {
    int err;
    struct dir_stack *dir =
      resolve_dir(r, root, cwd ? dir_stack_upcast(cwd) : NULL, pathname,
		  symlink_limit, &err);
    
    if(dir) {
      *result = pl_pack(r, METHOD_R_CAP, "c", dir_stack_downcast(dir));
    }
    else {
      *result = pl_pack(r, METHOD_FAIL, "i", err);
    }
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}

static void plpy_fs_make_node(cap_t obj, region_t r, struct cap_args args,
			      struct cap_args *result)
{
  if(pl_unpack(r, args, METHOD_FS_MAKE_NODE, "")) {
    *result = pl_pack(r, METHOD_R_CAP, "c", (cap_t) fs_make_empty_node());
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}

static void plpy_fs_attach_at_path(cap_t obj1, region_t r, struct cap_args args,
				   struct cap_args *result)
{
  cap_t root, obj;
  fs_node_t root_node;
  seqf_t pathname;
  if(pl_unpack(r, args, METHOD_FS_ATTACH_AT_PATH, "cSc",
	       &root, &pathname, &obj) &&
     (root_node = fs_node_upcast(root))) {
    int err;
    if(fs_attach_at_pathname(root_node, NULL /* cwd */, pathname,
			     inc_ref(obj), &err) < 0) {
      *result = pl_pack(r, METHOD_FAIL, "i", err);
    }
    else {
      *result = pl_pack(r, METHOD_OKAY, "");
    }
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}

static void plpy_fs_resolve_populate(cap_t obj1, region_t r, struct cap_args args,
				     struct cap_args *result)
{
  cap_t root_dir, root_node, cwd;
  seqf_t pathname;
  int flags;
  if(pl_unpack(r, args, METHOD_FS_RESOLVE_POPULATE, "ccdiS",
	       &root_dir, &root_node, &cwd, &flags, &pathname) &&
     fs_node_upcast(root_node) &&
     (cwd == NULL || dir_stack_upcast(cwd)))
  {
    int err;
    if(fs_resolve_populate(root_dir, fs_node_upcast(root_node),
			   cwd ? dir_stack_upcast(cwd) : NULL,
			   pathname, flags, &err) < 0) {
      *result = pl_pack(r, METHOD_FAIL, "i", err);
    }
    else {
      *result = pl_pack(r, METHOD_OKAY, "");
    }
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}

static void plpy_fs_dir_of_node(cap_t obj1, region_t r, struct cap_args args,
				struct cap_args *result)
{
  cap_t arg;
  if(pl_unpack(r, args, METHOD_FS_DIR_OF_NODE, "c", &arg) &&
     fs_node_upcast(arg)) {
    *result = pl_pack(r, METHOD_R_CAP, "c",
		      fs_make_root(fs_node_upcast(arg)));
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}

static void plpy_fs_print_tree(cap_t obj1, region_t r, struct cap_args args,
			       struct cap_args *result)
{
  cap_t node;
  if(pl_unpack(r, args, METHOD_FS_PRINT_TREE, "c", &node) &&
     fs_node_upcast(node)) {
    fs_print_tree(0, fs_node_upcast(node));
    *result = pl_pack(r, METHOD_OKAY, "");
  }
  else {
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
  pl_args_free(&args);
}


DECLARE_VTABLE(defaults_vtable);
#include "out-vtable-defaults.h"

struct fun_wrapper {
  struct filesys_obj hdr;
  struct filesys_obj_vtable vtable;
};
cap_t wrap_function(void (*fun)(cap_t obj, region_t r, struct cap_args args,
				struct cap_args *result))
{
  struct fun_wrapper *obj =
    filesys_obj_make(sizeof(struct fun_wrapper),
		     NULL /* vtable */);
  obj->hdr.vtable = &obj->vtable;
  obj->vtable = defaults_vtable;
  obj->vtable.cap_call = fun;
  return (cap_t) obj;
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
  
  { "cap_close_all_connections", plpy_cap_close_all_connections, METH_NOARGS,
    "Drop all the obj-cap-protocol connections that are currently open\n"
    "by closing the file descriptors.  This is for calling in a\n"
    "newly-forked process as soon as it is created.  If this is not done,\n"
    "both processes will try to use the connection (if only to drop object\n"
    "references), likely leading to a protocol violation." },

  { "kernel_execve", plpy_libc_kernel_execve, METH_VARARGS,
    "Calls the kernel's execve() system call, avoiding interception by\n"
    "Plash's libc." },
  
  { "libc_duplicate_connection", plpy_libc_duplicate_connection, METH_NOARGS,
    "Duplicates the connection that libc has with the server.\n"
    "Returns a file descriptor for the connection." },
  
  { NULL, NULL, 0, NULL }  /* Sentinel */
};


void initplash(void)
{
  PyObject *mod;

  if(PyType_Ready(&plpy_fd_type) < 0) { return; }
  if(PyType_Ready(&plpy_obj_type) < 0) { return; }

  plpy_init();

  mod = Py_InitModule3("plash", module_methods,
		       "Plash module.");

  Py_INCREF(&plpy_fd_type);
  PyModule_AddObject(mod, "FD", (PyObject *) &plpy_fd_type);
  Py_INCREF(&plpy_obj_type);
  PyModule_AddObject(mod, "Plash", (PyObject *) &plpy_obj_type);

  Py_INCREF(plpy_wrapper_class);
  PyModule_AddObject(mod, "Wrapper", (PyObject *) plpy_wrapper_class);
  Py_INCREF(plpy_pyobj_class);
  PyModule_AddObject(mod, "Pyobj", (PyObject *) plpy_pyobj_class);

#define ADD_FUNCTION(py_name, c_function) \
  PyModule_AddObject(mod, py_name, plpy_obj_to_py(wrap_function(c_function)))

  ADD_FUNCTION("resolve_dir", plpy_resolve_dir);
  ADD_FUNCTION("fs_make_node", plpy_fs_make_node);
  ADD_FUNCTION("fs_attach_at_path", plpy_fs_attach_at_path);
  ADD_FUNCTION("fs_resolve_populate", plpy_fs_resolve_populate);
  ADD_FUNCTION("fs_dir_of_node", plpy_fs_dir_of_node);
  ADD_FUNCTION("fs_print_tree", plpy_fs_print_tree);
  ADD_FUNCTION("cap_make_connection", plpy_make_conn2);
}
