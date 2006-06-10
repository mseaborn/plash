
/* Grab the pointer and output mouse events that are received. */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main()
{
  Display *dpy;

  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }

  XGrabPointer(dpy, DefaultRootWindow(dpy),
	       0 /* owner_events */,
	       ButtonPressMask | ButtonReleaseMask,
	       GrabModeAsync /* pointer_mode */,
	       GrabModeAsync /* keyboard_mode */,
	       None /* confine_to */,
	       None /* cursor; note that "None" makes the pointer
		       disappear while over the window */,
	       CurrentTime);
  
  while(1) {
    XEvent ev;
    XNextEvent(dpy, &ev);
    switch(ev.type) {
      case ButtonPress:
	printf("button press\n");
	break;

      case ButtonRelease:
	printf("button release\n");
	break;

      default:
	printf("other event\n");
	break;
    }
  }
  return 0;
}
