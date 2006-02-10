/* Copyright (C) 2004 Mark Seaborn

   This file is part of Plash, the Principle of Least Authority Shell.

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

#include "cap-utils.h"


int expect_ok(struct cap_args args)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_str(&ok, &data, "Okay");
  m_end(&ok, &data);
  if(ok && args.caps.size == 0 && args.fds.count == 0) {
    region_free(r);
    return 0;
  }
  region_free(r);
  caps_free(args.caps);
  close_fds(args.fds);
  return -1;
}

int expect_cap1(struct cap_args args, cap_t *c)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_str(&ok, &data, "Okay");
  m_end(&ok, &data);
  if(ok && args.caps.size == 1 && args.fds.count == 0) {
    *c = args.caps.caps[0];
    region_free(r);
    return 0;
  }
  region_free(r);
  caps_free(args.caps);
  close_fds(args.fds);
  return -1;
}

int expect_fd1(struct cap_args args, int *fd)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_str(&ok, &data, "Okay");
  m_end(&ok, &data);
  if(ok && args.caps.size == 0 && args.fds.count == 1) {
    *fd = args.fds.fds[0];
    region_free(r);
    return 0;
  }
  region_free(r);
  caps_free(args.caps);
  close_fds(args.fds);
  return -1;
}

/* Returns -1 on error. */
int unpack_exec_args(region_t r, struct arg_m_buf argbuf, bufref_t args_ref,
		     struct exec_args *ea)
{
  int i;
  int size;
  const bufref_t *a;

  ea->argv = 0;
  ea->env = 0;
  ea->fds = 0;
  ea->fds_count = 0;
  ea->root_dir = 0;
  ea->got_cwd = 0;
  ea->pgid = 0;
  
  // arg_print(stderr, &argbuf, args_ref);

  if(argm_array(&argbuf, args_ref, &size, &a)) return -1;
  for(i = 0; i < size; i++) {
    bufref_t tag_ref, arg_ref;
    seqf_t tag;
    if(argm_pair(&argbuf, a[i], &tag_ref, &arg_ref) ||
       argm_str(&argbuf, tag_ref, &tag)) return -1;
    
    if(seqf_equal(tag, seqf_string("Argv"))) {
      int i;
      int count;
      const bufref_t *a;
      if(argm_array(&argbuf, arg_ref, &count, &a)) return -1;
      ea->argv = region_alloc(r, (count + 1) * sizeof(char *));
      for(i = 0; i < count; i++) {
	seqf_t str;
	if(argm_str(&argbuf, a[i], &str)) return -1;
	ea->argv[i] = region_strdup_seqf(r, str);
      }
      ea->argv[count] = 0;
    }
    else if(seqf_equal(tag, seqf_string("Env."))) {
      int i;
      int count;
      const bufref_t *a;
      if(argm_array(&argbuf, arg_ref, &count, &a)) return -1;
      ea->env = region_alloc(r, (count + 1) * sizeof(char *));
      for(i = 0; i < count; i++) {
	seqf_t str;
	if(argm_str(&argbuf, a[i], &str)) return -1;
	ea->env[i] = region_strdup_seqf(r, str);
      }
      ea->env[count] = 0;
    }
    else if(seqf_equal(tag, seqf_string("Fds."))) {
      int i;
      const bufref_t *a;
      if(argm_array(&argbuf, arg_ref, &ea->fds_count, &a)) return -1;
      ea->fds = region_alloc(r, ea->fds_count * sizeof(struct fd_mapping));
      for(i = 0; i < ea->fds_count; i++) {
	bufref_t no_ref, fd_ref;
	if(argm_pair(&argbuf, a[i], &no_ref, &fd_ref) ||
	   argm_int(&argbuf, no_ref, &ea->fds[i].fd_no) ||
	   argm_fd(&argbuf, fd_ref, &ea->fds[i].fd)) return -1;
      }
    }
    else if(seqf_equal(tag, seqf_string("Root"))) {
      if(argm_cap(&argbuf, arg_ref, &ea->root_dir)) return -1;
    }
    else if(seqf_equal(tag, seqf_string("Cwd."))) {
      if(argm_str(&argbuf, arg_ref, &ea->cwd)) return -1;
      ea->got_cwd = 1;
    }
    else if(seqf_equal(tag, seqf_string("Pgid"))) {
      if(argm_int(&argbuf, arg_ref, &ea->pgid)) return -1;
    }
    else return -1;
  }
  return 0;
}


int parse_cap_list(seqf_t list, seqf_t *elt, seqf_t *rest)
{
  if(list.size > 0) {
    int i = 0;
    while(i < list.size) {
      if(list.data[i] == ';') {
	elt->data = list.data;
	elt->size = i;
	rest->data = list.data + i + 1;
	rest->size = list.size - i - 1;
	return 1;
      }
      i++;
    }
    elt->data = list.data;
    elt->size = i;
    rest->data = 0;
    rest->size = 0;
    return 1;
  }
  else return 0;
}
