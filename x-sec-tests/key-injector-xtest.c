
/* Fakes key press events using the XTest extension.  The key presses
   are treated by X as if they were entered through the keyboard, so
   are processed according to the current input focus, keyboard grabs,
   etc.  Unlike key presses sent using SendEvent, X clients can't
   ignore these. */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>


int main(void)
{
  Display *dpy = XOpenDisplay(0);
  if(!dpy) {
    fprintf(stderr, "Can't open display\n");
    return 1;
  }
  while(1) {
    unsigned keycode = XKeysymToKeycode(dpy, XStringToKeysym("h"));
    /* We must send both key press and key release events.
       If we send just a key press, autorepeat kicks in. */
    XTestFakeKeyEvent(dpy, keycode, 1 /* is_press */, 0 /* delay */);
    XTestFakeKeyEvent(dpy, keycode, 0 /* is_press */, 0 /* delay */);
    XFlush(dpy);
    if(sleep(1) < 0) { perror("sleep"); }
  }
  return 0;
}
