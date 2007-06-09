/* Copyright (C) 2004, 2005, 2006 Mark Seaborn

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
#include <errno.h>
#include <fcntl.h>

#include "region.h"
#include "filesysobj.h"
#include "resolve-filename.h"


/* This function for opening executable files can generate a couple of
   warnings.  The warnings are sent to stderr, which for the server
   won't be redirected. */
int open_executable_file(struct filesys_obj *obj, seqf_t cmd_filename, int *err)
{
  int read_perm_missing = FALSE;
  int fd;
  
  /* If this is a setuid executable, warn that setuid is not supported. */
  /* This check is not security critical, so it doesn't matter that
     it's subject to a race condition. */
  {
    struct stat st;
    if(obj->vtable->fsobj_stat(obj, &st, err) >= 0) {
      if((st.st_mode & S_IROTH) == 0) {
	read_perm_missing = TRUE;
      }
      if(st.st_mode & (S_ISUID | S_ISGID)) {
	region_t r = region_make();
	fprintf(stderr,
		_("plash: warning: setuid/gid bit not honoured on `%s'\n"),
		region_strdup_seqf(r, cmd_filename));
	region_free(r);
      }
    }
  }
  fd = obj->vtable->open(obj, O_RDONLY, err);

  /* Plash's behaviour is different from the usual Unix behaviour in
     that it can't run executables whose read permission bit is not set.
     Give this warning only if opening the file really does fail. */
  if(fd < 0 && *err == EACCES && read_perm_missing) {
    region_t r = region_make();
    fprintf(stderr, _("plash: warning: can't execute file without read permission: `%s'\n"),
	    region_strdup_seqf(r, cmd_filename));
    region_free(r);
  }
  return fd;
}


/* Checks for executables that are scripts using the `#!' syntax. */
/* This is not done recursively.  If there's a script that says it should
   be executed using another script, that won't work.  This is the
   behaviour of Linux.  There didn't seem much point in generalising the
   mechanism. */
/* Takes an FD for an executable.  The initial executable is looked up
   in the user's namespace.  But pathnames specified in the `#!' line
   are looked up in the process's namespace. */
/* If exec_fd_out is non-NULL, it will return a file descriptor for the
   executable (the interpreter executable if #! is used) -- this is for
   use with ld.so's --fd option (which I am removing).
   Otherwise, it returns the filename of the executable via
   exec_filename_out. */
/* Takes ownership of the FD it's given (ie. either closes it, or returns
   it via exec_fd_out if applicable). */
/* Returns -1 if there's an error. */
int exec_for_scripts
  (region_t r,
   struct filesys_obj *root, struct dir_stack *cwd,
   const char *cmd, int exec_fd, int argc, const char **argv,
   int *exec_fd_out, const char **exec_filename_out,
   int *argc_out, const char ***argv_out,
   int *err)
{
  char buf[1024];
  int got = 0;
  
  while(got < sizeof(buf)) {
    int x = read(exec_fd, buf + got, sizeof(buf) - got);
    if(x < 0) { *err = errno; return -1; }
    if(x == 0) break;
    got += x;
  }

  /* No whitespace is allowed before the "#!" */
  if(got >= 2 && buf[0] == '#' && buf[1] == '!') {
    int icmd_start, icmd_end;
    int arg_start, arg_end;
    int i = 2;
    seqf_t icmd;
    region_t r2;
    struct filesys_obj *obj;

    close(exec_fd);

    /* Parse the #! line to find the interpreter filename, and an
       optional argument for it. */
    while(i < got && buf[i] == ' ') i++; /* Skip spaces */
    if(i >= got) { *err = EINVAL; return -1; }
    icmd_start = i;
    while(i < got && buf[i] != ' ' && buf[i] != '\n') i++; /* Skip to space */
    if(i >= got) { *err = EINVAL; return -1; }
    icmd_end = i;
    while(i < got && buf[i] == ' ') i++; /* Skip spaces */
    if(i >= got) { *err = EINVAL; return -1; }
    arg_start = i;
    while(i < got && buf[i] != '\n') i++; /* Skip to end of line */
    if(i >= got) { *err = EINVAL; return -1; }
    arg_end = i;

    /* Deal with the interpreter executable's filename. */
    icmd.data = buf + icmd_start;
    icmd.size = icmd_end - icmd_start;
    if(exec_fd_out) {
      int fd;
      r2 = region_make();
      obj = resolve_file(r2, root, cwd, icmd, SYMLINK_LIMIT,
			 FALSE /* nofollow */, err);
      region_free(r2);
      if(!obj) return -1; /* Error */
      fd = obj->vtable->open(obj, O_RDONLY, err);
      filesys_obj_free(obj);
      if(fd < 0) return -1; /* Error */

      *exec_fd_out = fd;
    }
    else {
      assert(exec_filename_out);
      *exec_filename_out = region_strdup_seqf(r, icmd);
    }

    if(arg_end > arg_start) {
      seqf_t arg = { buf + arg_start, arg_end - arg_start };
      int i;
      const char **argv2 = region_alloc(r, (argc + 2) * sizeof(char *));
      argv2[0] = region_strdup_seqf(r, icmd);
      argv2[1] = region_strdup_seqf(r, arg);
      argv2[2] = cmd;
      for(i = 1; i < argc; i++) argv2[i+2] = argv[i];
      *argc_out = argc + 2;
      *argv_out = argv2;
      return 0;
    }
    else {
      int i;
      const char **argv2 = region_alloc(r, (argc + 2) * sizeof(char *));
      argv2[0] = region_strdup_seqf(r, icmd);
      argv2[1] = cmd;
      for(i = 1; i < argc; i++) argv2[i+1] = argv[i];
      *argc_out = argc + 1;
      *argv_out = argv2;
      return 0;
    }
  }
  else {
    /* Assume an ELF executable. */
    if(exec_fd_out) {
      *exec_fd_out = exec_fd;
    }
    else {
      close(exec_fd);
      *exec_filename_out = cmd;
    }
    *argc_out = argc;
    *argv_out = argv;
    return 0;
  }
}

/* Parses PATH.  If it contains any elements, returns TRUE and sets
   pathname to the first element, and rest to the rest. */
static int parse_path(seqf_t path, seqf_t *pathname, seqf_t *rest)
{
  if(path.size > 0) {
    int i = 0;
    while(i < path.size) {
      if(path.data[i] == ':') {
	pathname->data = path.data;
	pathname->size = i;
	rest->data = path.data + i + 1;
	rest->size = path.size - i - 1;
	return TRUE;
      }
      i++;
    }
    pathname->data = path.data;
    pathname->size = i;
    rest->data = 0;
    rest->size = 0;
    return TRUE;
  }
  else return FALSE;
}

/* Look up executable name in PATH if it doesn't contain '/'.
   Returns -1 if executable was not found. */
int resolve_executable_name(region_t r,
			    struct filesys_obj *root, struct dir_stack *cwd,
			    const char *filename,
			    const char **result)
{
  if(strchr(filename, '/')) {
    *result = filename;
    return 0; /* OK */
  }
  else {
    /* Search PATH for the command. */
    const char *path1;
    seqf_t path, path_entry;
    
    path1 = getenv("PATH");
    if(!path1) { return -1; }
    path = seqf_string(path1);
    
    while(parse_path(path, &path_entry, &path)) {
      seqf_t full_pathname =
	flatten0(r, cat3(r, mk_leaf(r, path_entry),
			 mk_string(r, "/"),
			 mk_string(r, filename)));
      int err;
      struct filesys_obj *obj =
	resolve_file(r, root, cwd, full_pathname, SYMLINK_LIMIT,
		     FALSE /* nofollow */, &err);
      if(obj) {
	filesys_obj_free(obj);
	*result = full_pathname.data;
	return 0; /* OK */
      }
    }
    return -1; /* Not found */
  }
}
