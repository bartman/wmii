/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
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
static void handle_maprequest(XEvent * e);
static void handle_motionnotify(XEvent * e);
static void handle_propertynotify(XEvent * e);
static void handle_unmapnotify(XEvent * e);
static void handle_enternotify(XEvent * e);
static void handle_ignore_enternotify_crap(XEvent *e);

static unsigned int ignore_enternotify_crap = 0;

void (*handler[LASTEvent]) (XEvent *);

void init_event_hander()
{
	int i;
	/* init event handler */
	for (i = 0; i < LASTEvent; i++) {
		handler[i] = 0;
	}
	handler[ButtonPress] = handle_buttonpress;
	handler[ConfigureRequest] = handle_configurerequest;
	handler[ConfigureNotify] = handle_ignore_enternotify_crap;
	handler[CirculateNotify] = handle_ignore_enternotify_crap;
	handler[DestroyNotify] = handle_destroynotify;
	handler[EnterNotify] = handle_enternotify;
	handler[Expose] = handle_expose;
	handler[GravityNotify] = handle_ignore_enternotify_crap;
	handler[MapRequest] = handle_maprequest;
	handler[MotionNotify] = handle_motionnotify;
	handler[PropertyNotify] = handle_propertynotify;
	handler[MapNotify] = handle_ignore_enternotify_crap;
	handler[UnmapNotify] = handle_unmapnotify;
}

void check_event(Connection * c)
{
	XEvent ev;
	while (XPending(dpy)) { /* main evet loop */
		XNextEvent(dpy, &ev);
		if (handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}

static void handle_buttonpress(XEvent * e)
{
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;
	Frame *f = win_to_frame(ev->window);

	if (f) {
		handle_frame_buttonpress(ev, f);
		return;
	}
	if (ev->window == root) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XSync(dpy, False);
		return;
	}
	if ((c = win_to_client(ev->window))) {
		if (c->frame) { /* client is attached */
			ev->state &= valid_mask;
			if (ev->state & Mod1Mask) {
				Align align;
				XRaiseWindow(dpy, c->frame->win);
				switch (ev->button) {
				case Button1:
					mouse_move(c->frame);
					break;
				case Button3:
					align = xy_to_align(&c->rect, ev->x, ev->y);
					if (align == CENTER) mouse_move(c->frame);
					else mouse_resize(c->frame, align);
					break;
				default:
					break;
				}
			}
		}
	}
}

static void handle_configurerequest(XEvent * e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	Client *c;
	unsigned int bw = 0, tabh = 0;
	Frame *f = 0;

	/* fprintf(stderr, "%s",  "configure request\n"); */
	c = win_to_client(ev->window);
	ev->value_mask &= ~CWSibling;

	if (c) {
		/* fprintf(stderr, "%s",  "configure request client\n"); */
		f = c->frame;

		if (f) {
			bw = border_width(f);
			tabh = tab_height(f);
		}
		if (ev->value_mask & CWStackMode) {
			if (wc.stack_mode == Above) XRaiseWindow(dpy, c->win);
			else ev->value_mask &= ~CWStackMode;
		}
		gravitate(c, tabh ? tabh : bw, bw, 1);

		if (ev->value_mask & CWX) c->rect.x = ev->x;
		if (ev->value_mask & CWY) c->rect.y = ev->y;
		if (ev->value_mask & CWWidth) c->rect.width = ev->width;
		if (ev->value_mask & CWHeight) c->rect.height = ev->height;
		if (ev->value_mask & CWBorderWidth) c->border = ev->border_width;

		gravitate(c, tabh ? tabh : bw, bw, 0);

		if (f) {
			f->rect.x = wc.x = c->rect.x - bw;
			f->rect.y = wc.y = c->rect.y - (tabh ? tabh : bw);
			f->rect.width = wc.width = c->rect.width + 2 * bw;
			f->rect.height = wc.height = c->rect.height + bw + (tabh ? tabh : bw);
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = ev->detail;
			XConfigureWindow(dpy, f->win, ev->value_mask, &wc);
			configure_client(c);
		}
	}
	wc.x = ev->x;
	wc.y = ev->y;

	if (f) {
		/* if so, then bw and tabh are already initialized */
		wc.x = bw;
		wc.y = tabh ? tabh : bw;
	}
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
	XConfigureWindow(dpy, e->xconfigurerequest.window, ev->value_mask, &wc);
	XSync(dpy, False);

	/* fprintf(stderr, "%d,%d,%d,%d\n", wc.x, wc.y, wc.width, wc.height); */
}

static void handle_destroynotify(XEvent * e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	Client *c = win_to_client(ev->window);
	/* fprintf(stderr, "destroy: client 0x%x\n", (int)ev->window); */
	if (c) {
		c->destroyed = True;
		detach_client(c);
	}
}

static void handle_expose(XEvent * e)
{
	static Frame *f;
	if (e->xexpose.count == 0) {
		f = win_to_frame(e->xbutton.window);
		if (f)
			draw_frame(f, nil);
	}
}

static void handle_maprequest(XEvent * e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	static XWindowAttributes wa;
	static Client *c;

	/* fprintf(stderr, "map: window 0x%x\n", (int)ev->window); */
	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	/* there're client which send map requests twice */
	c = win_to_client(ev->window);
	if (!c)
		c = alloc_client(ev->window);
	if (!c->frame) {
		init_client(c, &wa);
		attach_client(c);
	}
}

static void handle_motionnotify(XEvent * e)
{
	Frame *f = win_to_frame(e->xmotion.window);
	Cursor cursor;
	if (f) {
		cursor = cursor_for_motion(f, e->xmotion.x, e->xmotion.y);
		if (cursor != f->cursor) {
			f->cursor = cursor;
			XDefineCursor(dpy, f->win, cursor);
		}
	}
}

static void handle_propertynotify(XEvent * e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client *c = win_to_client(ev->window);

	if (c)
		handle_client_property(c, ev);
}

static void handle_unmapnotify(XEvent * e)
{
	XUnmapEvent *ev = &e->xunmap;
	Client *c;
	handle_ignore_enternotify_crap(e);
	if ((c = win_to_client(ev->window)))
		detach_client(c);
}

static void handle_enternotify(XEvent * e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;

	if ((ev->mode != NotifyNormal) || (ev->detail == NotifyInferior))
		return;

	c = win_to_client(ev->window);
	if (c && c->frame && (ev->serial != ignore_enternotify_crap)) {
		Frame *old = get_sel_frame();
		if (old != c->frame) {
			sel_frame(c->frame, 1);
			draw_frame(old, nil);
			draw_frame(c->frame, nil);
		}
	}
}

static void handle_ignore_enternotify_crap(XEvent *e)
{
	ignore_enternotify_crap = e->xany.serial;
	XSync(dpy, False);
}

