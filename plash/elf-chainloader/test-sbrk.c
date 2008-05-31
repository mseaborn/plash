
#include <string.h>
#include <unistd.h>


/* We need to make the executable bigger than the chainloader in order
   to reproduce the problem of the heap getting overwritten by the
   executable.  This creates a large bss, which is sufficient. */
char data[4096 * 10];

int main()
{
    int size = 4096;
    void *memory = sbrk(size);
    if(memory == (void *) -1)
	return 1;
    /* Check that we can write to the allocated block. */
    memset(memory, 0, size);
    return 0;
}
