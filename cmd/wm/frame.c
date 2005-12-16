/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void select_client(void *obj, char *arg);
static void handle_after_write_frame(IXPServer *s, File *file);
static void handle_before_read_frame(IXPServer *s, File *file);

/* action table for /frame/?/ namespace */
Action frame_acttbl[] = {
	{"select", select_client},
	{0, 0}
};

Frame *alloc_frame(XRectangle * r)
{
	XSetWindowAttributes wa;
	static int id = 0;
	char buf[MAX_BUF];
	Frame *f = (Frame *) cext_emallocz(sizeof(Frame));
	int bw, th;

	f->rect = *r;
	f->cursor = normal_cursor;

	snprintf(buf, MAX_BUF, "/detached/frame/%d", id);
	f->file[F_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client", id);
	f->file[F_CLIENT_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client/sel", id);
	f->file[F_SEL_CLIENT] = ixp_create(ixps, buf);
	f->file[F_SEL_CLIENT]->bind = 1;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/ctl", id);
	f->file[F_CTL] = ixp_create(ixps, buf);
	f->file[F_CTL]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/geometry", id);
	f->file[F_GEOMETRY] = ixp_create(ixps, buf);
	f->file[F_GEOMETRY]->before_read = handle_before_read_frame;
	f->file[F_GEOMETRY]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/border", id);
	f->file[F_BORDER] = wmii_create_ixpfile(ixps, buf, def[WM_BORDER]->content);
	f->file[F_BORDER]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/tab", id);
	f->file[F_TAB] = wmii_create_ixpfile(ixps, buf, def[WM_TAB]->content);
	f->file[F_TAB]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/handleinc", id);
	f->file[F_HANDLE_INC] = wmii_create_ixpfile(ixps, buf, def[WM_HANDLE_INC]->content);
	f->file[F_HANDLE_INC]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/locked", id);
	f->file[F_LOCKED] = wmii_create_ixpfile(ixps, buf, def[WM_LOCKED]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/bgcolor", id);
	f->file[F_SEL_BG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/fgcolor", id);
	f->file[F_SEL_FG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/bordercolor", id);
	f->file[F_SEL_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/bgcolor", id);
	f->file[F_NORM_BG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/fgcolor", id);
	f->file[F_NORM_FG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/bordercolor", id);
	f->file[F_NORM_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b2press", id);
	f->file[F_EVENT_B2PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B2PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b3press", id);
	f->file[F_EVENT_B3PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B3PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b4press", id);
	f->file[F_EVENT_B4PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B4PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b5press", id);
	f->file[F_EVENT_B5PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B5PRESS]->content);
	id++;

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = SubstructureRedirectMask | ExposureMask | ButtonPressMask | PointerMotionMask;

    bw 	= border_width(f);
	th = tab_height(f);
	f->rect.width += 2 * bw;
	f->rect.height += bw + (th ? th : bw);
	f->win = XCreateWindow(dpy, root, f->rect.x, f->rect.y, f->rect.width,
							f->rect.height, 0, DefaultDepth(dpy, screen_num),
							CopyFromParent, DefaultVisual(dpy, screen_num),
							CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(dpy, f->win, f->cursor);
	f->gc = XCreateGC(dpy, f->win, 0, 0);
	XSync(dpy, False);
	return f;
}

Frame *win_to_frame(Window w)
{
	Page *p;
	for (p = pages; p; p = p->next) {
		Frame *f;
		for (f = p->managed->layout->frames(p->managed); f; f = f->next)
			if (f->win == w)
				return f;
		for (f = p->floating->layout->frames(p->floating); f; f = f->next)
			if (f->win == w)
				return f;
	}
	return nil;
}

void destroy_frame(Frame * f)
{
	XFreeGC(dpy, f->gc);
	XDestroyWindow(dpy, f->win);
	ixp_remove_file(ixps, f->file[F_PREFIX]);
	free(f);
}

unsigned int tab_height(Frame * f)
{
	if (blitz_strtonum(f->file[F_TAB]->content, 0, 1))
		return font->ascent + font->descent + 4;
	return 0;
}

unsigned int border_width(Frame * f)
{
	if (blitz_strtonum(f->file[F_BORDER]->content, 0, 1))
		return BORDER_WIDTH;
	return 0;
}

static void check_dimensions(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client *c = sel_client();
	if (!c)
		return;

	if (c->size.flags & PMinSize) {
		if (f->rect.width - 2 * bw < c->size.min_width) {
			f->rect.width = c->size.min_width + 2 * bw;
		}
		if (f->rect.height - bw - (tabh ? tabh : bw) < c->size.min_height) {
			f->rect.height = c->size.min_height + bw + (tabh ? tabh : bw);
		}
	}
	if (c->size.flags & PMaxSize) {
		if (f->rect.width - 2 * bw > c->size.max_width) {
			f->rect.width = c->size.max_width + 2 * bw;
		}
		if (f->rect.height - bw - (tabh ? tabh : bw) > c->size.max_height) {
			f->rect.height = c->size.max_height + bw + (tabh ? tabh : bw);
		}
	}
}

static void resize_incremental(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client *c = f->sel;
	if (!c)
		return;
	/* increment stuff, see chapter 4.1.2.3 of the ICCCM Manual */
	if (c->size.flags & PResizeInc) {
		int base_width = 0, base_height = 0;

		if (c->size.flags & PBaseSize) {
			base_width = c->size.base_width;
			base_height = c->size.base_height;
		} else if (c->size.flags & PMinSize) {
			/* base_{width,height} default to min_{width,height} */
			base_width = c->size.min_width;
			base_height = c->size.min_height;
		}
		/* client_width = base_width + i * c->size.width_inc for an integer i */
		f->rect.width -=
			(f->rect.width - 2 * bw - base_width) % c->size.width_inc;
		f->rect.height -=
			(f->rect.height - bw - (tabh ? tabh : bw) -
			 base_height) % c->size.height_inc;
	}
}

void resize_frame(Frame * f, XRectangle * r, XPoint * pt)
{
	Area *a = f->area;
	unsigned int tabh = tab_height(f);
	unsigned int bw = border_width(f);
	Client *c;

	a->layout->resize(f, r, pt);

	/* resize if client requests special size */
	check_dimensions(f, tabh, bw);

	if (f->file[F_HANDLE_INC]->content && ((char *) f->file[F_HANDLE_INC]->content)[0] == '1')
		resize_incremental(f, tabh, bw);

	XMoveResizeWindow(dpy, f->win, f->rect.x, f->rect.y, f->rect.width, f->rect.height);

	for (c = f->clients; c; c = c->next) {
		c->rect.x = bw;
		c->rect.y = tabh ? tabh : bw;
		c->rect.width = c->frame->rect.width - 2 * bw;
		c->rect.height = c->frame->rect.height - bw - (tabh ? tabh : bw);
		XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
		configure_client(c);
	}
}

/**
 * Assumes following file:
 *
 * ./sel-style/text-font	   "<value>"
 * ./sel-style/text-size	   "<int>"
 * ./sel-style/text-color	  "#RRGGBBAA"
 * ./sel-style/bg-color		"#RRGGBBAA"
 * ./sel-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 * ./norm-style/text-font	   "<value>"
 * ./norm-style/text-size	   "<int>"
 * ./norm-style/text-color	  "#RRGGBBAA"
 * ./norm-style/bg-color		"#RRGGBBAA"
 * ./norm-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 */
void draw_frame(Frame *f)
{
	Draw d = { 0 };
	int bw = border_width(f);
	XRectangle notch;
	if (bw) {
		notch.x = bw;
		notch.y = bw;
		notch.width = f->rect.width - 2 * bw;
		notch.height = f->rect.height - 2 * bw;
		d.drawable = f->win;
		d.font = font;
		d.gc = f->gc;

		/* define ground plate (i = 0) */
		if (f == sel_frame()) {
			d.bg = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_BORDER_COLOR]->content);
		} else {
			d.bg = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_BORDER_COLOR]->content);
		}
		d.rect = f->rect;
		d.rect.x = d.rect.y = 0;
		d.notch = &notch;

		blitz_drawlabel(dpy, &d);
	}
	draw_clients(f);
	XSync(dpy, False);
}

void handle_frame_buttonpress(XButtonEvent *e, Frame *f)
{
	Align align;
	int bindex, cindex = e->x / (f->rect.width / f->nclients);
	f->sel = clientat(f->clients, cindex);
	if (e->button == Button1) {
		align = cursor_to_align(f->cursor);
		if (align == CENTER)
			mouse_move(f);
		else
			mouse_resize(f, align);
		f->area->layout->focus(f, False);
		return;
	}
	f->area->layout->focus(f, False);
	bindex = F_EVENT_B2PRESS - 2 + e->button;
	/* frame mouse handling */
	if (f->file[bindex]->content)
		wmii_spawn(dpy, f->file[bindex]->content);
}

void attach_client_to_frame(Frame *f, Client *client)
{
	Client *c;
	wmii_move_ixpfile(c->file[C_PREFIX], f->file[F_CLIENT_PREFIX]);
	f->file[F_SEL_CLIENT]->content = client->file[C_PREFIX]->content;
	for (c = f->clients; c && c->next; c = c->next);
	if (!c) {
		f->clients = client;
		client->prev = client->next = nil;
	}
	else {
		client->prev = c;
		client->next = nil;
		c->next = client;
	}
	f->sel = client;
	client->frame = f;
	resize_frame(f, &f->rect, 0);
	reparent_client(client, f->win, client->rect.x, client->rect.y);
	show_client(client);
	sel_client(client);
}

void detach_client_from_frame(Client *c, Bool unmap)
{
	Frame *f = c->frame;

	c->frame = nil;
	f->file[F_SEL_CLIENT]->content = nil;
	wmii_move_ixpfile(c->file[C_PREFIX], def[WM_DETACHED_CLIENT]);
	
	if (f->sel == c) {
		if (c->prev)
			f->sel = c->prev;
		else
			f->sel = nil;
	}

	if (f->clients == c) {
		if (c->next)
			c->next->prev = nil;
		f->clients = c->next;
	}
	else {
		c->prev->next = c->next;
		if (c->next)
			c->next->prev = c->prev;
	}

	if (!f->sel)
		f->sel = f->clients;
		
	if (!c->destroyed) {
		if (!unmap) {
			attach_detached(c);
			hide_client(c);
		}
		c->rect.x = f->rect.x;
		c->rect.y = f->rect.y;
		reparent_client(c, root, c->rect.x, c->rect.y);
	}
	if (f->sel) {
		focus_client(f->sel);
		f->area->layout->focus(f, False);
	}
}

static void select_client(void *obj, char *arg)
{
	Client *c;
	Frame *f = obj;
	if (!f || !arg || !f->clients->next)
		return;
	if (!strncmp(arg, "prev", 5))
		c = f->sel->prev;
	else if (!strncmp(arg, "next", 5))
		c = f->sel->next;
	else
		c = clientat(f->clients, blitz_strtonum(arg, 0, f->nclients - 1));
	focus_client(c);
	f->area->layout->focus(f, False);
}

static Frame *handle_before_read_frames(IXPServer *s, File *file, Area *a)
{
	Frame *f;
	char buf[64];
	for (f = a->layout->frames(a); f; f = f->next)
		if (file == f->file[F_GEOMETRY]) {
			snprintf(buf, 64, "%d,%d,%d,%d", f->rect.x, f->rect.y, f->rect.width, f->rect.height);
			if (file->content)
				free(file->content);
			file->content = cext_estrdup(buf);
			file->size = strlen(buf);
			return f;
		}
	return nil;
}

static void handle_before_read_frame(IXPServer *s, File *file)
{
	Page *p;
	for (p = pages; p; p = p->next) {
		if (handle_before_read_frames(s, file, p->managed)
			|| handle_before_read_frames(s, file, p->floating))
			return;
	}
}

static Frame *handle_after_write_frames(IXPServer *s, File *file, Area *a)
{
	Frame *f;
	for (f = a->layout->frames(a); f; f = f->next) {
		if (file == f->file[F_CTL]) {
			run_action(file, f, frame_acttbl);
			return f;
		}
		if (file == f->file[F_TAB] || file == f->file[F_BORDER] || file == f->file[F_HANDLE_INC]) {
			f->area->layout->arrange(f->area);
			return f;
		} else if (file == f->file[F_GEOMETRY]) {
			char *geom = f->file[F_GEOMETRY]->content;
			if (geom && strrchr(geom, ',')) {
				XRectangle frect = f->rect;
				blitz_strtorect(&rect, &frect, geom);
				resize_frame(f, &frect, 0);
			}
			return f;
		}
	}
	return nil;
}

static void handle_after_write_frame(IXPServer *s, File *file)
{
	Page *p;
	for (p = pages; p; p = p->next) {
		if (handle_after_write_frames(s, file, p->managed)
			|| handle_after_write_frames(s, file, p->floating))
			return;
	}
}

Frame *sel_frame()
{
	Area *a = sel_area();
	if (!a)
		return nil;
	return a->layout->sel(a);
}

Frame *get_bottom_frame(Frame *frames)
{
	Frame *f;
	for (f = frames; f && f->next; f = f->next);
	return f;
}
