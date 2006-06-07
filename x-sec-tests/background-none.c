
/* Open a window with a background colour of None, and leave the
   window's contents undefined. */

#include <stdio.h>
#include <X11/Xlib.h>

int main()
{
  Display *dpy;
  Screen *scn;
  Window win;
  
  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  scn = XDefaultScreenOfDisplay(dpy);
  win = XCreateWindow(dpy, XDefaultRootWindow(dpy),
		      100, 100, /* pos */
		      200, 200, /* size */
		      1, XDefaultDepthOfScreen(scn),
		      InputOutput,
		      XDefaultVisualOfScreen(scn),
		      0,
		      NULL);
  XMapWindow(dpy, win);
  XFlush(dpy);

  /* Do nothing but keep the X connection open. */
  while(1) {
    XEvent ev;
    XNextEvent(dpy, &ev);
  }
  return 0;
}
