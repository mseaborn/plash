
/* Grab the keyboard and output the keypress events that are received.
   Enter q to quit! */

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

  printf("Grabbing keyboard. Press 'q' to exit.\n");
  XGrabKeyboard(dpy, DefaultRootWindow(dpy),
		0 /* owner_events */,
		GrabModeAsync /* pointer_mode */,
		GrabModeAsync /* keyboard_mode */,
		CurrentTime);
  
  while(1) {
    XEvent ev;
    XNextEvent(dpy, &ev);
    switch(ev.type) {
      case KeyPress:
      case KeyRelease:
	{
	  char buf[2];
	  int got = XLookupString(&ev.xkey, buf, sizeof(buf), 0, 0);
	  buf[1] = 0;

	  printf("key %s %s\n",
		 ev.type == KeyPress ? "press:  " : "release:",
		 got > 0 ? buf : "none");

	  if(got > 0 && buf[0] == 'q') { return 0; }
	  
	  break;
	}

      default:
	printf("other event\n");
    }
  }
  return 0;
}
