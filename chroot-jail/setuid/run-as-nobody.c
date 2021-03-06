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

#include <alloca.h>
#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

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

    if(setgroups(0, 0) < 0) { perror("setgroups"); return 1; }
    if(setgid(pwd->pw_gid) < 0) { perror("setgid"); return 1; }
    if(setuid(pwd->pw_uid) < 0) { perror("setuid"); return 1; }
    execve(argv[1], argv2, envp);
    perror("exec");
    return 1;
  }
}
