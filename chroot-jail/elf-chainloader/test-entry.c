
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void *chainloader_main(void *unused_stack)
{
    char *str = "Entering C function works\n";
    write(1, str, strlen(str));
    exit(123);
    return NULL;
}
