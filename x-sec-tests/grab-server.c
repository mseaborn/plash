
/* Grab the X server for 5 seconds. */

#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

int main()
{
  Display *dpy;
  int delay = 5;
  
  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  XGrabServer(dpy);
  XFlush(dpy);
  fprintf(stderr, "Grabbing server for %i seconds...\n", delay);
  if(sleep(delay) < 0) { perror("sleep"); }

  XUngrabServer(dpy); /* Just in case. */
  XFlush(dpy);
  
  return 0;
}
