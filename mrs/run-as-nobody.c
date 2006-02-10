
#include <stdio.h>
#include <unistd.h>
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
