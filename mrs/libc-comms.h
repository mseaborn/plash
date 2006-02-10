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

#ifndef libc_comms_h
#define libc_comms_h

#include "region.h"
#include "comms.h"
#include "filesysobj.h"


int new_open(const char *filename, int flags, ...);


#define __set_errno(x) (errno = (x))

extern int comm_sock;
extern struct comm *comm;
extern cap_t fs_server, conn_maker, fs_op_maker;

int plash_init(void);
int send_req(region_t r, seqt_t msg);
int get_reply(seqf_t *msg, fds_t *fds);
int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds);
int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds);
int req_and_reply(region_t r, seqt_t msg, seqf_t *reply);



/* Used to define version names like GLIBC_2_2 */
#include "../abi-versions.h"

/* from glibc's include/libc-symbols.h */

/* #define weak_alias(name, aliasname) \
     extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));
*/
#define weak_alias(name, aliasname) \
  extern int aliasname() __attribute ((weak, alias (#name)));

#define weak_extern(symbol) asm (".weak " #symbol);


/* from glibc's include/shlib-compat.h */

# define versioned_symbol(lib, local, symbol, version) \
  versioned_symbol_1 (local, symbol, VERSION_##lib##_##version)
# define versioned_symbol_1(local, symbol, name) \
  default_symbol_version (local, symbol, name)

/* from glibc's include/libc-symbols.h */

#  define default_symbol_version(real, name, version) \
     __asm__ (".symver " #real "," #name "@@" #version)


#define offsetof(s, f) ((int) &((s *) 0)->f)



#endif
