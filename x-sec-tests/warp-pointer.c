
/* Use WarpPointer request, moving the pointer around in a circle. */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

int main()
{
  Display *dpy;
  int x = 0, y = 0;
  float angle = 0;
  
  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  while(1) {
    int size = 100;
    int x2 = size * sin(angle);
    int y2 = size * cos(angle);
    XWarpPointer(dpy, None, None, 0, 0, 0, 0, x2 - x, y2 - y);
    XFlush(dpy);
    {
      struct timespec t;
      t.tv_sec = 0;
      t.tv_nsec = 100000;
      nanosleep(&t, NULL);
    }
    
    angle += 0.03;
    x = x2;
    y = y2;
  }
  return 0;
}
