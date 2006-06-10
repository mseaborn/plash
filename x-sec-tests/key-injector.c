
/* Repeatedly send a keypress event to the window with the current
   input focus.

   Note that the "send_event" bit will be set in these events, and
   some X clients (e.g. xterm) will notice this and ignore the
   event. */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <X11/Xlib.h>


Display *dpy;

void inject_keypress(void)
{
  XEvent ev;
  Window win;
  int revert_to_return;
  
  XGetInputFocus(dpy, &win, &revert_to_return);
  // printf("win 0x%x\n", win);

  ev.xkey.type = KeyPress;
  ev.xkey.serial = 0;
  ev.xkey.send_event = 1;
  ev.xkey.display = dpy;
  ev.xkey.window = win;
  ev.xkey.root = win;
  ev.xkey.subwindow = win;
  ev.xkey.time = 0;
  ev.xkey.x = 0;
  ev.xkey.y = 0;
  ev.xkey.x_root = 0;
  ev.xkey.y_root = 0;
  ev.xkey.state = 0;
  ev.xkey.keycode = XKeysymToKeycode(dpy, XStringToKeysym("h"));
  // printf("keycode %i\n", ev.xkey.keycode);
  XSendEvent(dpy, win, 1 /* propagate */, KeyPressMask, &ev);
}

int main(void)
{
  dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  while(1) {
    inject_keypress();
    XFlush(dpy);
    if(sleep(1) < 0) { perror("sleep"); }
  }
  return 0;
}
