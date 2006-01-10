/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "cext.h"

static char *version[] = {
    "wmiiwarp - window manager improved warp - " VERSION "\n"
        " (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr,
            "usage: wmiiwarp [-v] <x> <y>\n"
            "      -v     version info\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    Display *dpy;
    int x, y;
    const char *errstr;

    /* command line args */
    if(argc < 2)
        usage();
    if(!strncmp(argv[1], "-v", 2)) {
        fprintf(stdout, "%s", version[0]);
        exit(0);
    }
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiiwarp: cannot open display\n");
        exit(1);
    }
    if((argc == 2) && !strncmp(argv[1], "center", 7)) {
        x = DisplayWidth(dpy, DefaultScreen(dpy)) / 2;
        y = DisplayHeight(dpy, DefaultScreen(dpy)) / 2;
    } else if(argc == 3) {
	x = cext_strtonum(argv[1], 0, DisplayWidth(dpy, DefaultScreen(dpy)), &errstr);
	if(errstr) {
		fprintf(stderr, "wmiiwarp: invalid x value: '%s'\n", errstr);
		usage();
	}
	y = cext_strtonum(argv[2], 0, DisplayHeight(dpy, DefaultScreen(dpy)), &errstr);
	if(errstr) {
		fprintf(stderr, "wmiiwarp: invalid y value: '%s'\n", errstr);
		usage();
	}
    }
    XWarpPointer(dpy, None, RootWindow(dpy, DefaultScreen(dpy)),
                 0, 0, 0, 0, x, y);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XCloseDisplay(dpy);
    return 0;
}
