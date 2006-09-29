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

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

#include "cap-protocol.h"


struct hellow_obj {
  struct filesys_obj hdr;
};

void hellow_obj_call(struct filesys_obj *obj, region_t r,
		     struct cap_args args, struct cap_args *result)
{
  printf("hellow was invoked\n");

  result->data = mk_string(r, "hellow's reply");
  result->caps = caps_empty;
  result->fds = fds_empty;
}

void hellow_obj_free(struct filesys_obj *obj)
{
}

struct filesys_obj_vtable hellow_obj_vtable = {
  /* .free = */ hellow_obj_free,

  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ hellow_obj_call,
  /* .single_use = */ 0,
  
  /* .type = */ objt_unknown,
  /* .stat = */ dummy_stat,
  /* .utimes = */ dummy_utimes,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_socket_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .symlink = */ dummy_symlink,
  /* .rename = */ dummy_rename_or_link,
  /* .link = */ dummy_rename_or_link,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .socket_bind = */ dummy_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};



int main()
{
  int socks[2];
  int pid;
  
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("socketpair");
    return 1;
  }

  pid = fork();
  if(pid == 0) {
    region_t r = region_make();
    cap_t *caps, hello;
    struct cap_args result;
    
    close(socks[0]);
    caps = cap_make_connection(r, socks[1], caps_empty, 1, "a");
    hello = caps[0];

    printf("1: calling\n");
    hello->vtable->cap_call(hello, r,
			    cap_args_make(mk_string(r, "hello world"),
					  caps_empty, fds_empty),
			    &result);
    printf("1: returned\n");
    exit(0);
  }
  if(pid < 0) { perror("fork"); exit(1); }

  {
    region_t r = region_make();
    struct hellow_obj *hw;
    close(socks[1]);

    hw = filesys_obj_make(sizeof(struct hellow_obj), &hellow_obj_vtable);
    
    cap_make_connection(r, socks[0], mk_caps1(r, (cap_t) hw), 0, "b");
    filesys_obj_free((cap_t) hw);
    printf("2: starting server\n");
    cap_run_server();
    printf("2: server finished\n");
  }
  return 0;
}
