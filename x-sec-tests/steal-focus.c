
/* This program opens a window and repeatedly grabs the input focus
   for that window. */

#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

int main()
{
  Display *dpy;
  Screen *scn;
  Window win;
  XSetWindowAttributes attr;

  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  scn = XDefaultScreenOfDisplay(dpy);
  attr.background_pixel = XWhitePixelOfScreen(scn);
  win = XCreateWindow(dpy, XDefaultRootWindow(dpy),
		      100, 100, /* pos */
		      200, 200, /* size */
		      1 /* border_width */, XDefaultDepthOfScreen(scn),
		      InputOutput,
		      XDefaultVisualOfScreen(scn),
		      CWBackPixel /* valuemask */,
		      &attr);
  XMapWindow(dpy, win);
  XFlush(dpy);
  
  while(1) {
    XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
    XFlush(dpy);
    sleep(1);
  }
  return 0;
}
