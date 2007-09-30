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


void set_errno_from_reply(seqf_t msg);
void set_errno_from_result(region_t r, struct cap_args result);

int new_open(const char *filename, int flags, ...);


int my_atoi(const char *str);

int libc_get_fs_op(cap_t *result);


extern int comm_sock;
extern struct comm *comm;
extern cap_seq_t process_caps;
extern char *process_caps_names;
extern cap_t fs_server, conn_maker, fs_op_maker;
extern int libc_debug;

int plash_init(void);
int send_req(region_t r, seqt_t msg);
int get_reply(seqf_t *msg, fds_t *fds);
int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds);
int req_and_reply(region_t r, seqt_t msg, seqf_t *reply);
void libc_log(const char *msg);


#ifdef GLIBC_SEPARATE_BUILD

#define weak_extern(symbol) \
  extern typeof(symbol) symbol __attribute((weak));
#define weak_alias(name, aliasname) \
  extern int aliasname() __attribute ((weak, alias (#name)));


/* Symbol export declarations */

/* NB. Semicolons not necessary */

/* export_weak_alias() corresponds to weak_alias(),
   export_versioned_symbol() corresponds to versioned_symbol(), and
   export_compat_symbol() corresponds to compat_symbol()
   from glibc's include/shlib-compat.h. */

/* e.g. export(new_open, __open) */
#define export(name, aliasname) \
  extern int export_##aliasname() __attribute ((alias (#name)));

/* e.g. export_weak_alias(new_open, open) */
#define export_weak_alias(name, aliasname) \
  extern int export_##aliasname() __attribute ((weak, alias (#name)));

/* This declares the default version of a symbol.
   e.g. export_versioned_symbol(libc, new_readdir64, readdir64, GLIBC_2_2) */
#define export_versioned_symbol(lib, local, symbol, version) \
  pl_versioned_symbol_1(local, symbol, VERSION_##lib##_##version)
#define pl_versioned_symbol_1(local, symbol, version) \
  extern int exportver_##symbol##__defaultversion__##version() \
    __attribute ((alias (#local)));

/* This declares a non-default version of a symbol.
   e.g. export_compat_symbol(libc, new_readdir64, readdir64, GLIBC_2_1) */
#define export_compat_symbol(lib, local, symbol, version) \
  pl_compat_symbol_1(local, symbol, VERSION_##lib##_##version)
#define pl_compat_symbol_1(local, symbol, version) \
  extern int exportver_##symbol##__version__##version() \
    __attribute ((alias (#local)));

#include "abi-versions.h"

/* Copied from glibc's shlib-compat.h.
   Test for IS_IN_##lib was removed. */
#define SHLIB_COMPAT(lib, introduced, obsoleted) \
   (!(ABI_##lib##_##obsoleted - 0) \
    || ((ABI_##lib##_##introduced - 0) < (ABI_##lib##_##obsoleted - 0)))

#else

#include "abi-versions.h"
#include "libc-symbols.h"
#include "shlib-compat.h"

#define export_weak_alias(name, aliasname) \
  weak_alias(name, aliasname)

#define export(name, aliasname) \
  strong_alias(name, aliasname)


#if !defined(IS_IN_rtld)

/* A simpler definition would be:
     versioned_symbol(lib, local, symbol, version)
   but the assembler complains that a symbol has more than one version,
   so as a workaround, define the versioned symbol via a temporary alias. */
#define export_versioned_symbol(lib, local, symbol, version) \
  strong_alias(local, __workaround_##local##_##version); \
  versioned_symbol(lib, __workaround_##local##_##version, symbol, version)
#define export_compat_symbol(lib, local, symbol, version) \
  compat_symbol(lib, local, symbol, version)

#else

#define export_versioned_symbol(lib, local, symbol, version) \
  strong_alias(local, symbol)

#endif

#endif /* GLIBC_SEPARATE_BUILD */


#include <pthread.h>
#include <errno.h>

extern pthread_mutex_t libc_lock;

weak_extern(pthread_mutex_lock)
weak_extern(pthread_mutex_unlock)
weak_extern(pthread_mutex_trylock)

inline static void plash_libc_lock(void)
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

inline static void plash_libc_unlock(void)
{
  if(pthread_mutex_unlock) {
    pthread_mutex_unlock(&libc_lock);
  }
}


#endif
