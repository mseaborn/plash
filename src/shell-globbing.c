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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

/* To do:
   handle "." and ".."
   sort contents

   NB. dot-files *are* included in the globbed list.
   I think the concept of hiding dot-files should be phased out.
*/

#include <sys/types.h>
#include <errno.h>
#include <regex.h>
#include <pwd.h>
#include <unistd.h>

#include "filesysobj.h"
#include "resolve-filename.h"
#include "shell.h"
#include "shell-variants.h"
#include "shell-globbing.h"


/* If a glob pattern doesn't contain any wildcards, it can be converted
   directly into a pathname.  That's what this function does.
   It returns -1 if the glob pattern does contain wildcards, and couldn't
   be converted. */
int filename_of_glob_path(region_t r, struct glob_path *filename, seqt_t *result)
{
  seqt_t got;
  
  struct path_start *start;
  struct glob_path_aux *path;
  struct char_cons *user_name;
  
  if(!m_glob_path(filename, &start, &path)) assert(0);

  if(m_start_root(start)) {
    got = mk_string(r, "/");
    if(m_glob_path_end(path, 0)) { *result = got; return 0; }
  }
  else if(m_start_cwd(start)) {
    if(m_glob_path_end(path, 0)) { *result = mk_string(r, "."); return 0; }
    got = mk_string(r, "");
  }
  else if(m_start_home(start, &user_name)) {
    struct passwd *pwd;
    if(user_name) {
      /* Pathname of the form "~USER" or "~USER/..." */
      seqf_t user_name1 = flatten_charlist(r, user_name);
      pwd = getpwnam(user_name1.data);
    }
    else {
      /* Pathname of the form "~" or "~/..." */
      uid_t uid = getuid(); /* always succeeds */
      pwd = getpwuid(uid);
    }

    if(pwd && pwd->pw_dir) {
      if(m_glob_path_end(path, 0)) {
	*result = mk_string(r, pwd->pw_dir);
	return 0;
      }
      got = cat2(r, mk_string(r, pwd->pw_dir), mk_string(r, "/"));
    }
    /* FIXME: an unknown user name shouldn't case fallback to globbing */
    else return -1; /* Error */
  }
  else {
    assert(0); return -1;
  }

  while(1) {
    struct char_cons *leaf, *p;
    struct glob_path_aux *rest;
    int slash;
    
    if(!m_glob_path_cons(path, &leaf, &rest)) assert(0);
    
    for(p = leaf; p; p = p->next) {
      if(p->c == GLOB_STAR) return -1; /* Error */
    }

    if(m_glob_path_end(rest, &slash)) {
      seqt_t x = cat2(r, got, mk_leaf(r, flatten_charlist(r, leaf)));
      *result = slash ? cat2(r, x, mk_string(r, "/")) : x;
      return 0;
    }
    else {
      got = cat3(r, got, mk_leaf(r, flatten_charlist(r, leaf)),
		 mk_string(r, "/"));
      path = rest;
    }
  }
}

/* Convert a glob pattern into a regular expression. */
/* It's a shame no way is provided for building up regexp syntax trees,
   rather than building strings that then have to be parsed. */
/* Returns non-zero for failure. */
int compile_glob_pattern(int *pat, int len, regex_t *result)
{
  region_t r = region_make();
  char *regexp = region_alloc(r, 2 + len*2 + 1);
  int i, j;
  int rc;

  regexp[0] = '^';
  j = 1;
  for(i = 0; i < len; i++) {
    switch(pat[i]) {
      case GLOB_STAR:
	regexp[j + 0] = '.';
	regexp[j + 1] = '*';
	j += 2;
	break;
	
      case '.': case '\\':
      case '*': case '+': case '?':
      case '|': case '^': case '$':
      case '(': case ')':
      case '[': case ']':
	regexp[j + 0] = '\\';
	regexp[j + 1] = pat[i];
	j += 2;
	break;
	
      default:
	regexp[j] = pat[i];
	j++;
	break;
    }
  }
  regexp[j] = '$';
  regexp[j + 1] = 0;
  rc = regcomp(result, regexp, REG_NOSUB);
  region_free(r);
  return rc;
}

void glob_aux(region_t r,
	      struct filesys_obj *root, struct dir_stack *dirstack,
	      seqt_t pathname_got, struct glob_path_aux *path,
	      struct glob_params *params);
void glob_aux2(region_t r,
	       struct filesys_obj *root, struct dir_stack *dirstack,
	       char *name1,
	       seqt_t pathname_got, struct glob_path_aux *rest,
	       struct glob_params *params);

void glob_resolve(region_t r,
		  struct filesys_obj *root, struct dir_stack *cwd,
		  struct glob_path *filename,
		  struct glob_params *params)
{
  struct dir_stack *dirstack;
  seqt_t pathname_got;
  
  struct path_start *start;
  struct glob_path_aux *path;
  struct char_cons *user_name;
  if(!m_glob_path(filename, &start, &path)) assert(0);

  if(m_start_root(start)) {
    dirstack = dir_stack_root(root);
    pathname_got = mk_string(r, "/");
  }
  else if(m_start_cwd(start)) {
    if(!cwd) return; /* Error, but not reported */
    dirstack = cwd;
    dirstack->hdr.refcount++;
    pathname_got = mk_string(r, "");
  }
  else if(m_start_home(start, &user_name)) {
    struct passwd *pwd;
    if(user_name) {
      /* Pathname of the form "~USER" or "~USER/..." */
      seqf_t user_name1 = flatten_charlist(r, user_name);
      pwd = getpwnam(user_name1.data);
      /* pathname_got = cat3(r, mk_string(r, "~"), mk_leaf(r, user_name1),
			  mk_string(r, "/")); */
    }
    else {
      /* Pathname of the form "~" or "~/..." */
      uid_t uid = getuid(); /* always succeeds */
      pwd = getpwuid(uid);
      /* pathname_got = mk_string(r, "~/"); */
    }

    if(pwd && pwd->pw_dir) {
      int err;
      dirstack = resolve_dir(r, root, cwd, seqf_string(pwd->pw_dir),
			     SYMLINK_LIMIT, &err);
      if(!dirstack) return; /* Error */
      pathname_got = cat2(r, mk_string(r, pwd->pw_dir), mk_string(r, "/"));
    }
    else return; /* Error */
  }
  else {
    assert(0);
    return;
  }

  if(m_glob_path_end(path, 0)) {
    /* FIXME: handle root */
  }
  else {
    glob_aux(r, root, dirstack, pathname_got, path, params);
  }
}

static int compare_strings(const void *x, const void *y)
{
  return strcmp(*(char **) x, *(char **) y);
}

void glob_aux(region_t r,
	      struct filesys_obj *root, struct dir_stack *dirstack,
	      seqt_t pathname_got, struct glob_path_aux *path,
	      struct glob_params *params)
{
  struct char_cons *leaf1, *p;
  struct glob_path_aux *rest;
  int is_glob;
  int leaf_len;
  if(!m_glob_path_cons(path, &leaf1, &rest)) assert(0);

  is_glob = 0;
  for(p = leaf1, leaf_len = 0; p; p = p->next) {
    if(p->c == GLOB_STAR) is_glob = 1;
    leaf_len++;
  }

  /* If the pathname component contains any wildcards, we need to search
     through the directory's contents. */
  if(is_glob) {
    int *leaf;
    int i;
    regex_t regexp;
    region_t r2;
    seqt_t list;
    seqf_t buf;
    int err;
    struct str_list *got;
    int got_count;
    char **got_array;

    /* Copy list representing glob pattern into an array. */
    leaf = region_alloc(r, leaf_len * sizeof(int));
    for(p = leaf1, i = 0; p; p = p->next, i++) { leaf[i] = p->c; }

    if(compile_glob_pattern(leaf, leaf_len, &regexp) != 0) {
      assert(0);
      return; /* Error */
    }

    /* Read directory contents. */
    r2 = region_make();
    if(dirstack->dir->vtable->list(dirstack->dir, r2, &list, &err) < 0) {
      region_free(r2);
      regfree(&regexp);
      dir_stack_free(dirstack);
      return; /* Error */
    }
    buf = flatten(r2, list);
    got = 0;
    got_count = 0;
    while(buf.size > 0) {
      int inode, type;
      seqf_t name;
      int ok = 1;
      m_int(&ok, &buf, &inode);
      m_int(&ok, &buf, &type);
      m_lenblock(&ok, &buf, &name);
      if(ok) {
	char *name1 = strdup_seqf(name);
	if(regexec(&regexp, name1, 0 /* nmatch */, 0 /* pmatch */,
		   0 /* eflags */) == 0) {
	  struct str_list *l = region_alloc(r2, sizeof(struct str_list));
	  l->str = name1;
	  l->next = got;
	  got = l;
	  got_count++;
	}
	else free(name1);
      }
      else {
	region_free(r2);
	regfree(&regexp);
	dir_stack_free(dirstack);
	return; /* Error */
      }
    }
    regfree(&regexp);

    /* Copy list of matched names into an array. */
    {
      int i;
      struct str_list *l;
      got_array = amalloc(got_count * sizeof(char *));
      for(l = got, i = 0; l; l = l->next, i++) {
	assert(i < got_count);
	got_array[i] = l->str;
      }
    }
    region_free(r2);
    /* Sort the array. */
    qsort(got_array, got_count, sizeof(char *), compare_strings);
    /* Process each matched name. */
    for(i = 0; i < got_count; i++) {
      dirstack->hdr.refcount++;
      glob_aux2(r, root, dirstack, got_array[i], pathname_got, rest, params);
    }
    free(got_array);
    dir_stack_free(dirstack);
  }
  else {
    /* If there are no wildcards, it's simple. */
    seqf_t name = flatten_charlist(r, leaf1);
    /*
    if(filename_parent(name)) {
    }
    else if(filename_samedir(name)) {
    }
    */
    char *name1 = strdup_seqf(name);
    glob_aux2(r, root, dirstack, name1, pathname_got, rest, params);
  }
}

/* Receives name1 as an owning reference. */
void glob_aux2(region_t r,
	       struct filesys_obj *root, struct dir_stack *dirstack,
	       char *name1,
	       seqt_t pathname_got, struct glob_path_aux *rest,
	       struct glob_params *params)
{
  int slash;
  struct filesys_obj *obj =
    dirstack->dir->vtable->traverse(dirstack->dir, name1);
  if(!obj) {
    free(name1);
    dir_stack_free(dirstack);
    return;
  }

  if(m_glob_path_end(rest, &slash)) {
    struct str_list *l;
    seqf_t name = seqf_string(name1);
    
    if(slash) {
      int obj_type = obj->vtable->type(obj);
      /* Check that it's a directory. */
      if(obj_type == OBJT_SYMLINK) {
	/* Check that the symlink destination resolves to a directory. */
	int err;
	struct dir_stack *new_stack;
	seqf_t link_dest;
	if(obj->vtable->readlink(obj, r, &link_dest, &err) < 0) {
	  filesys_obj_free(obj);
	  free(name1);
	  dir_stack_free(dirstack);
	  return; /* Error */
	}
	new_stack = resolve_dir(r, root, dirstack, link_dest, SYMLINK_LIMIT, &err);
	if(!new_stack) return; /* Error */
	dir_stack_free(new_stack);
      }
      else if(obj_type != OBJT_DIR) {
	filesys_obj_free(obj);
	free(name1);
	dir_stack_free(dirstack);
	return; /* Error */
      }
    }
    
    /* Add this filename to the list. */
    l = region_alloc(r, sizeof(struct str_list));
    l->str = flatten_str(r, cat3(r, pathname_got, mk_leaf(r, name),
				 mk_string(r, slash ? "/" : "")));
    l->next = 0;
    *params->got_end = l;
    params->got_end = &l->next;

    free(name1);
    filesys_obj_free(obj);
    dir_stack_free(dirstack);
  }
  else {
    seqf_t name = region_dup_seqf(r, seqf_string(name1));
    
    int obj_type = obj->vtable->type(obj);
    if(obj_type == OBJT_DIR) {
      dirstack = dir_stack_make(obj, dirstack, name1);
    }
    else if(obj_type == OBJT_SYMLINK) {
      int err;
      struct dir_stack *new_stack;
      seqf_t link_dest;
      free(name1);
      if(obj->vtable->readlink(obj, r, &link_dest, &err) < 0) {
	filesys_obj_free(obj);
	dir_stack_free(dirstack);
	return; /* Error */
      }
      filesys_obj_free(obj);
      new_stack = resolve_dir(r, root, dirstack, link_dest, SYMLINK_LIMIT, &err);
      dir_stack_free(dirstack);
      if(!new_stack) return; /* Error */
      dirstack = new_stack;
    }
    else {
      free(name1);
      dir_stack_free(dirstack);
      return; /* ENOTDIR */
    }
    glob_aux(r, root, dirstack,
	     cat3(r, pathname_got, mk_leaf(r, name), mk_string(r, "/")),
	     rest, params);
  }
}
