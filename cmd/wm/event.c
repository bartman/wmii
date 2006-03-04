/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

#include "wm.h"

/* local functions */
static void handle_buttonpress(XEvent * e);
static void handle_configurerequest(XEvent * e);
static void handle_destroynotify(XEvent * e);
static void handle_expose(XEvent * e);
static void handle_keypress(XEvent * e);
static void handle_keymapnotify(XEvent * e);
static void handle_maprequest(XEvent * e);
static void handle_motionnotify(XEvent * e);
static void handle_propertynotify(XEvent * e);
static void handle_unmapnotify(XEvent * e);
static void handle_clientmessage(XEvent * e);

void (*handler[LASTEvent]) (XEvent *);

void
init_x_event_handler()
{
    int i;
    /* init event handler */
    for(i = 0; i < LASTEvent; i++) {
        handler[i] = 0;
    }
    handler[ButtonPress] = handle_buttonpress;
    handler[ConfigureRequest] = handle_configurerequest;
    handler[DestroyNotify] = handle_destroynotify;
    handler[Expose] = handle_expose;
    handler[KeyPress] = handle_keypress;
    handler[KeymapNotify] = handle_keymapnotify;
    handler[MapRequest] = handle_maprequest;
    handler[MotionNotify] = handle_motionnotify;
    handler[PropertyNotify] = handle_propertynotify;
    handler[UnmapNotify] = handle_unmapnotify;
    handler[ClientMessage] = handle_clientmessage;
}

void
check_x_event(IXPConn *c)
{
    XEvent ev;
    while(XPending(dpy)) {      /* main evet loop */
        XNextEvent(dpy, &ev);
        if(handler[ev.type])
            (handler[ev.type]) (&ev);   /* call handler */
    }
}

static void
handle_buttonpress(XEvent *e)
{
    Client *c;
    XButtonPressedEvent *ev = &e->xbutton;
    Align align;
	static char buf[32];
	if(ev->window == winbar) {
		unsigned int i;
		for(i = 0; i < nlabel; i++)
			if(blitz_ispointinrect(ev->x, ev->y, &label[i]->rect)) {
				snprintf(buf, sizeof(buf), "LB %d %d\n", i, ev->button);
				write_event(buf);
			}
	}
	else if((c = win2clientframe(ev->window))) {
		if(ev->button == Button1) {
			if(sel_client() != c) {
				focus(c);
				return;
			}
			align = cursor2align(c->frame.cursor);
			if(align == CENTER)
				mouse_move(c);
			else 
				mouse_resize(c, align);
		}
	
		if(c && c->area) {
			snprintf(buf, sizeof(buf), "CB %d %d\n", client2index(c) + 1, ev->button);
			write_event(buf);
		}
	}
	else if((c = win2client(ev->window))) {
		ev->state &= valid_mask;
		if(ev->state & Mod1Mask) {
			XRaiseWindow(dpy, c->frame.win);
			switch (ev->button) {
				case Button1:
					focus(c);
					mouse_move(c);
					break;
				case Button3:
					focus(c);
					align = xy2align(&c->rect, ev->x, ev->y);
					if(align == CENTER)
						mouse_move(c);
					else
						mouse_resize(c, align);
					break;
			}
		}
		else if(ev->button == Button1)
			focus(c);

		if(c) {
			snprintf(buf, sizeof(buf), "CB %d %d\n", client2index(c) + 1, ev->button);
			write_event(buf);
		}
	}
	
}

static void
handle_configurerequest(XEvent *e)
{
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    Client *c;
    unsigned int bw = 0, bh = 0;

    c = win2client(ev->window);
	ev->value_mask &= ~CWSibling;

    if(c) {
        if(c->area) {
            bw = def.border;
            bh = bar_height();
        }

        gravitate(c, bh ? bh : bw, bw, 1);

        if(ev->value_mask & CWX)
            c->rect.x = ev->x;
        if(ev->value_mask & CWY)
            c->rect.y = ev->y;
        if(ev->value_mask & CWWidth)
            c->rect.width = ev->width;
        if(ev->value_mask & CWHeight)
            c->rect.height = ev->height;
        if(ev->value_mask & CWBorderWidth)
            c->border = ev->border_width;

        gravitate(c, bh ? bh : bw, bw, 0);

        if(c->area) {
            c->frame.rect.x = wc.x = c->rect.x - bw;
            c->frame.rect.y = wc.y = c->rect.y - (bh ? bh : bw);
            c->frame.rect.width = wc.width = c->rect.width + 2 * bw;
            c->frame.rect.height = wc.height = c->rect.height + bw + (bh ? bh : bw);
            wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = ev->detail;
            XConfigureWindow(dpy, c->frame.win, ev->value_mask, &wc);
            configure_client(c);
        }
    }

    wc.x = ev->x;
    wc.y = ev->y;
    if(c && c->area) {
        /* if so, then bw and bh are already initialized */
        wc.x = bw;
        wc.y = (bh ? bh : bw);
    }
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    XSync(dpy, False);

}

static void
handle_destroynotify(XEvent *e)
{
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client *c = win2client(ev->window);
    if(c) {
		fprintf(stderr, "destroy: %s\n", c->name);
        c->destroyed = True;
        detach_client(c, False);
    }
}

static void
handle_expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
    static Client *c;
    if(ev->count == 0) {
		if(ev->window == winbar) 
			draw_bar();
		else if((c = win2clientframe(ev->window)))
            draw_client(c);
    }
}

static void
handle_keypress(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;
	ev->state &= valid_mask;
    handle_key(root, ev->state, (KeyCode) ev->keycode);
}

static void
handle_keymapnotify(XEvent *e)
{
	unsigned int i;
	for(i = 0; i < nkey; i++) {
		ungrab_key(key[i]);
		grab_key(key[i]);
	}
}

static void
handle_maprequest(XEvent *e)
{
    XMapRequestEvent *ev = &e->xmaprequest;
    static XWindowAttributes wa;
    static Client *c;

    if(!XGetWindowAttributes(dpy, ev->window, &wa))
        return;
    if(wa.override_redirect) {
        XSelectInput(dpy, ev->window,
                     (StructureNotifyMask | PropertyChangeMask));
        return;
    }

    /* there're client which send map requests twice */
    c = win2client(ev->window);
    if(!c)
        c = alloc_client(ev->window, &wa);
    if(!c->area)
        attach_client(c);
}

static void
handle_motionnotify(XEvent *e)
{
    Client *c = win2clientframe(e->xmotion.window);
    if(c) {
    	Cursor cursor = cursor_for_motion(c, e->xmotion.x, e->xmotion.y);
        if(cursor != c->frame.cursor) {
            c->frame.cursor = cursor;
            XDefineCursor(dpy, c->frame.win, cursor);
        }
    }
}

static void
handle_propertynotify(XEvent *e)
{
    XPropertyEvent *ev = &e->xproperty;
    Client *c;

    if(ev->state == PropertyDelete)
        return;                 /* ignore */

    if((c = win2client(ev->window)))
        handle_client_property(c, ev);
}

static void
handle_unmapnotify(XEvent *e)
{
    XUnmapEvent *ev = &e->xunmap;
    Client *c;
    if((c = win2client(ev->window))) {
		fprintf(stderr, "unmap %s\n", c->name);
        detach_client(c, True);
	}
}

static void handle_clientmessage(XEvent *e)
{
    XClientMessageEvent *ev = &e->xclient;
	Client *c;

    if (ev->message_type == net_atom[NetNumWS] && ev->format == 32)
        return; /* ignore */
    else if (ev->message_type == net_atom[NetSelWS] && ev->format == 32) {
		int i = ev->data.l[0];
		if(i < ntag) {
			Tag *p = tag[i];
			focus_tag(p);
			if((c = sel_client_of_tag(p)))
				focus_client(c);
		}
    }
}
