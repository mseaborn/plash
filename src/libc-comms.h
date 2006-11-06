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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef libc_comms_h
#define libc_comms_h

#include "region.h"
#include "comms.h"
#include "filesysobj.h"
#include "libc-errno.h"


int new_open(const char *filename, int flags, ...);


int my_atoi(const char *str);


extern int comm_sock;
extern struct comm *comm;
extern cap_seq_t process_caps;
extern char *process_caps_names;
extern cap_t fs_server, conn_maker, fs_op_maker;
extern int libc_debug;

int plash_init(void);
int send_req(region_t r, seqt_t msg);
int get_reply(seqf_t *msg, fds_t *fds);
int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds);
int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds);
int req_and_reply(region_t r, seqt_t msg, seqf_t *reply);
void libc_log(const char *msg);


#define weak_extern(symbol) \
  extern typeof(symbol) symbol __attribute((weak));
#define weak_alias(name, aliasname) \
  extern int aliasname() __attribute ((weak, alias (#name)));


#define offsetof(s, f) ((int) &((s *) 0)->f)


#include <pthread.h>
#include <errno.h>

extern pthread_mutex_t libc_lock;

weak_extern(pthread_mutex_lock)
weak_extern(pthread_mutex_unlock)
weak_extern(pthread_mutex_trylock)

inline static void plash_libc_lock()
{
#if 0
  if(pthread_mutex_trylock && pthread_mutex_trylock(&libc_lock) == EBUSY) {
    const char *msg = "libc: contention\n";
    write(2, msg, strlen(msg));

    if(pthread_mutex_lock) pthread_mutex_lock(&libc_lock);
  }
#endif
  
  if(pthread_mutex_lock) pthread_mutex_lock(&libc_lock);
}

inline static void plash_libc_unlock()
{
  if(pthread_mutex_unlock) {
    pthread_mutex_unlock(&libc_lock);
  }
}


#endif
