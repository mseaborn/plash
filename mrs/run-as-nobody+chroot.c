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

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

#include "config.h"

/* This program is potentially more dangerous than run-as-nobody.

   Just changing the user and group IDs to "nobody" can't do any
   harm, unless there are some important files that are owned by
   "nobody", which should not be the case.
   (Well, the process would be able to access directories such as
   "/tmp" and create files owned by "nobody" there.
   Perhaps we could allocate a big block of user IDs and give
   each process a different user ID.  But they'd still be able to
   make their files in "/tmp" world-readable, and communicate with
   each other.)

   But if we do chroot, the chroot'ed process can exec setuid
   executables, which would load their dynamic linker and libraries
   *from the chroot filesystem*.  If a user has control of this
   filesystem, they can hard link setuid-root executables into it
   and get them to execute arbitrary code as root.

   However, we can make a chroot environment safe by not including
   "/lib", "/bin", "/etc" or any of the other directories through
   which setuid programs might access files that they depend on.
   (Hopefully they have safe fallback behaviour when those files
   don't exist!)
   So we can have a directory, say, "/usr/local/chroot-jail", which
   is owned by root.  It can contain a single directory, say
   "special", which can be owned by a normal user (other than root)
   who can modify its contents arbitrarily.  The path to the chroot
   jail, "/usr" and "/usr/local", must be entirely owned by root;
   if the user owned any part of it, they could replace the chroot
   jail with another directory.  It would not then be enough to
   check whether "/usr/local/chroot-jail" is owned by root: the user
   could have symlinked in some other directory, owned by root, that
   was built for some completely different purpose.
*/

/* We must read the user name while /etc/passwd is still accessible.
   We must do chroot while we're still running as root. */

int main(int argc, char *argv[], char *envp[])
{
  if(argc < 2) {
    printf("Usage: %s program args...\n", argv[0]);
    return 1;
  }
  else {
    char *user_name = "nobody";
    struct passwd *pwd;
    
    char **argv2 = alloca((argc-1+1) * sizeof(char *));
    int i;
    argv2[0] = argv[1];
    for(i = 1; i < argc-1; i++) argv2[i] = argv[i+1];
    argv2[i] = 0;

    pwd = getpwnam(user_name);
    if(!pwd) { fprintf(stderr, "Can't find user `%s'\n", user_name); return 1; }
    if(0) printf("uid = %i, gid = %i\n", pwd->pw_uid, pwd->pw_gid);

    if(chroot(JAIL_DIR) < 0) { perror("chroot"); return 1; }
    if(chdir("/") < 0) { perror("chdir"); return 1; }

    if(setgroups(0, 0) < 0) { perror("setgroups"); return 1; }
    if(setgid(pwd->pw_gid) < 0) { perror("setgid"); return 1; }
    if(setuid(pwd->pw_uid) < 0) { perror("setuid"); return 1; }
    
    execve(argv[1], argv2, envp);
    perror("exec");
    return 1;
  }
}
