
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stack.h"


static void debug(const char *str)
{
    write(1, str, strlen(str));
}

static void debug_str(const char *str)
{
    debug("\"");
    debug(str);
    debug("\"\n");
}

void *chainloader_main(struct args *args)
{
    char **p = args->stack.argv;
    debug("argv:\n");
    while(*p)
	debug_str(*p++);
    p++;
    debug("env:\n");
    while(*p)
	debug_str(*p++);
    p++;
    exit(123);
    return NULL;
}
