
/* Select for keypress events on the window that currently has the
   input focus, and print keypresses. */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main()
{
  Display *dpy;
  Window win;
  int revert_to_return;

  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  XGetInputFocus(dpy, &win, &revert_to_return);

  {
    XSetWindowAttributes attr;
    attr.event_mask = KeyPressMask | FocusChangeMask;
    XChangeWindowAttributes(dpy, win, CWEventMask, &attr);
  }

  while(1) {
    XEvent ev;
    XNextEvent(dpy, &ev);
    switch(ev.type) {
      /* When the pointer is inside the window, this program doesn't get
	 the events.  When the pointer is outside, it does! */
      case KeyPress:
      case KeyRelease:
	{
	  char buf[1];
	  int got = XLookupString(&ev.xkey, buf, sizeof(buf), 0, 0);
	  
	  if(ev.type == KeyPress) printf("press ");
	  else printf("release ");
	  
	  if(got > 0) { printf("key: %c\n", buf[0]); }
	  else { printf("key: none\n"); }
	  
	  break;
	}

      default:
	printf("other event\n");
    }
  }
}
