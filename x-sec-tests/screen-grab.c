
/* This program grabs a copy of the screen and displays it in a
   window.

   Note that if you use the XYPixmap image format instead of ZPixmap,
   this goes really slowly, and displays some interesting colour
   artifacts as the different colour layers are transferred to the X
   server and displayed. */

#include <stdio.h>
#include <X11/Xlib.h>

int main()
{
  Display *dpy;
  Screen *scn;
  XSetWindowAttributes attr;
  Window win;
  XImage *image;
  GC gc;
  
  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  scn = XDefaultScreenOfDisplay(dpy);

  image = XGetImage(dpy, XDefaultRootWindow(dpy),
		    0, 0,
		    WidthOfScreen(scn), HeightOfScreen(scn),
		    ~0 /* plane_mask */,
		    ZPixmap);
  if(!image) {
    fprintf(stderr, "XGetImage failed\n");
    return 1;
  }

  attr.event_mask = ExposureMask;
  win = XCreateWindow(dpy, XDefaultRootWindow(dpy),
		      100, 100, /* pos */
		      400, 400, /* size */
		      1 /* border_width */, XDefaultDepthOfScreen(scn),
		      InputOutput,
		      XDefaultVisualOfScreen(scn),
		      CWEventMask /* valuemask */,
		      &attr);
  XMapWindow(dpy, win);

  gc = XCreateGC(dpy, win, 0, NULL);
  
  while(1) {
    XEvent ev;
    XNextEvent(dpy, &ev);
    switch(ev.type) {
      case Expose:
	XPutImage(dpy, win, gc, image, 0, 0, 0, 0,
		  WidthOfScreen(scn), HeightOfScreen(scn));
	break;

      default:
	printf("other event\n");
	break;
    }
  }
  return 0;
}
