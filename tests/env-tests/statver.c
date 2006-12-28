
#include <sys/stat.h>
#include <stdio.h>

#define PRINT_VAL(x) \
  printf(#x "=%i\n", x);


void *f()
{
  return stat;
}

int main()
{
  struct stat st;
  
#ifdef _STAT_VER_KERNEL
  PRINT_VAL(_STAT_VER_KERNEL);
#endif
#ifdef _STAT_VER_SVR4
  PRINT_VAL(_STAT_VER_SVR4);
#endif
#ifdef _STAT_VER_LINUX
  PRINT_VAL(_STAT_VER_LINUX);
#endif
  
  PRINT_VAL(_STAT_VER);
  PRINT_VAL(sizeof(struct stat));
#ifdef TRY_STAT64
  PRINT_VAL(sizeof(struct stat64));
#endif

  PRINT_VAL(sizeof(st.st_ino));
  PRINT_VAL(sizeof(st.st_uid));
  PRINT_VAL(sizeof(st.st_mtim));
  PRINT_VAL(sizeof(st.st_mtime));
  
  return 0;
}
