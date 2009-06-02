/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

/* Here be dragons. */
/* Actually, I'm happy to say, the dragons have dissipated. */

enum {
	ButtonMask =
		ButtonPressMask | ButtonReleaseMask,
	MouseMask =
		ButtonMask | PointerMotionMask
};

static Handlers handlers;

enum { OHoriz, OVert };
typedef struct Framewin Framewin;
struct Framewin {
	/* Todo... give these better names. */
	Window* w;
	Rectangle grabbox;
	Frame*	f;
	Area*	ra;
	Point	pt;
	int	orientation;
	int	xy;
};

static Rectangle
framerect(Framewin *f) {
	Rectangle r;
	Point p;

	r.min = ZP;
	if(f->orientation == OHoriz) {
		r.max.x = f->xy;
		r.max.y = f->grabbox.max.y + f->grabbox.min.y;
	}else {
		r.max.x = f->grabbox.max.x + f->grabbox.min.x;
		r.max.y = f->xy;
		r = rectsubpt(r, Pt(Dx(r)/2, 0));
	}
	r = rectaddpt(r, f->pt);

	/* Keep onscreen */
	p = ZP;
	p.x -= min(0, r.min.x);
	p.x -= max(0, r.max.x - selscreen->r.max.x);
	p.y -= max(0, r.max.y - selscreen->brect.min.y - Dy(r)/2);
	return rectaddpt(r, p);
}

static void
frameadjust(Framewin *f, Point pt, int orientation, int xy) {
	f->orientation = orientation;
	f->xy = xy;
	f->pt = pt;
}

static Framewin*
framewin(Frame *f, Point pt, int orientation, int n) {
	WinAttr wa;
	Framewin *fw;

	fw = emallocz(sizeof *fw);
	wa.override_redirect = true;	
	wa.event_mask = ExposureMask;
	fw->w = createwindow(&scr.root, Rect(0, 0, 1, 1),
			scr.depth, InputOutput,
			&wa, CWEventMask);
	fw->w->aux = fw;
	sethandler(fw->w, &handlers);

	fw->f = f;
	fw->grabbox = f->grabbox;
	frameadjust(fw, pt, orientation, n);
	reshapewin(fw->w, framerect(fw));

	mapwin(fw->w);
	raisewin(fw->w);

	return fw;	
}

static void
framedestroy(Framewin *f) {
	destroywindow(f->w);
	free(f);
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Rectangle r;
	Framewin *f;
	Image *buf;
	CTuple *c;
	
	USED(e);

	f = w->aux;
	c = &def.focuscolor;
	buf = disp.ibuf;
	
	r = rectsubpt(w->r, w->r.min);
	fill(buf, r, c->bg);
	border(buf, r, 1, c->border);
	border(buf, f->grabbox, 1, c->border);
	border(buf, insetrect(f->grabbox, -f->grabbox.min.x), 1, c->border);

	copyimage(w, r, buf, ZP);	
}

static Handlers handlers = {
	.expose = expose_event,
};

static Area*
find_area(Point pt) {
	Area *a;
	int s;

	/* XXX: Multihead. Check this over. */
	for(s=0; s < nscreens; s++) {
		WMScreen *scrn = screens[s];
		View *v = screen_selview(scrn);
		if(!rect_haspoint_p(pt, scrn->r))
			continue;
		for(a=v->areas; a; a=a->next)
			if(pt.x < a->cr.max.x)
				return a;
	}
	return nil; /* XXX: Multihead. */
}

static void
vplace(Framewin *fw, Point pt) {
	Vector_long vec = {0};
	Rectangle r;
	Frame *f;
	Area *a;
	View *v;
	long l;
	int hr;

	/* TODO: this may need to be done for each visible view */
	v = selview();

	a = find_area(pt);
	if(a == nil)
		return; /* XXX: Multihead. */

	fw->ra = a;

	pt.x = a->cr.min.x;
	frameadjust(fw, pt, OHoriz, Dx(a->cr));

	r = fw->w->r;
	hr = Dy(r)/2;
	pt.y -= hr;

	if(a->frame == nil)
		goto done;

	vector_lpush(&vec, a->frame->cr.min.y);
	for(f=a->frame; f; f=f->anext) {
		if(f == fw->f)
			vector_lpush(&vec, f->cr.min.y + 0*hr);
		else if(f->collapsed)
			vector_lpush(&vec, f->cr.min.y + 1*hr);
		else
			vector_lpush(&vec, f->cr.min.y + 2*hr);
		if(!f->collapsed && f->anext != fw->f)
			vector_lpush(&vec, f->cr.max.y - 2*hr);
	}

	for(int i=0; i < vec.n; i++) {
		l = vec.ary[i];
		if(abs(pt.y - l) < hr) {
			pt.y = l;
			break;
		}
	}
	vector_lfree(&vec);

done:
	pt.x = a->cr.min.x;
	frameadjust(fw, pt, OHoriz, Dx(a->cr));
	reshapewin(fw->w, framerect(fw));
}

static void
hplace(Framewin *fw, Point pt) {
	Area *a;
	View *v;
	int minw;
	
	/* TODO: this may need to be done for each visible view */
	v = selview();

	a = find_area(pt);
	if(a == nil)
		return; /* XXX: Multihead. */

	fw->ra = nil;
	minw = Dx(view_rect(v))/NCOL;
	if(abs(pt.x - a->cr.min.x) < minw/2) {
		pt.x = a->cr.min.x;
		fw->ra = a->prev;
	}
	else if(abs(pt.x - a->cr.max.x) < minw/2) {
		pt.x = a->cr.max.x;
		fw->ra = a;
	}

	pt.y = a->cr.min.y;
	frameadjust(fw, pt, OVert, Dy(a->cr));
	reshapewin(fw->w, framerect(fw));	
}

static Point
grabboxcenter(Frame *f) {
	Point p;

	p = addpt(f->cr.min, f->grabbox.min);
	p.x += Dx(f->grabbox)/2;
	p.y += Dy(f->grabbox)/2;
	return p;
}

static int tvcol(Frame*);
static int thcol(Frame*);
static int tfloat(Frame*);

enum {
	TDone,
	TVCol,
	THCol,
	TFloat,
};

static int (*tramp[])(Frame*) = {
	0,
	tvcol,
	thcol,
	tfloat,
};

/* Trampoline to allow properly tail recursive move/resize routines.
 * We could probably get away with plain tail calls, but I don't
 * like the idea.
 */
static void
trampoline(int fn, Frame *f) {

	while(fn > 0) {
		resizing = fn != TFloat;
		view_update(f->view);
		warppointer(grabboxcenter(f));
		//f->collapsed = false;
		fn = tramp[fn](f);
	}
	ungrabpointer();
	resizing = false;
	view_update(f->view);
}

void
mouse_movegrabbox(Client *c, bool grabmod) {
	Frame *f;
	Point p;
	float x, y;

	f = c->sel;

	SET(x);
	SET(y);
	if(grabmod) {
		p = querypointer(f->client->framewin);
		x = (float)p.x / Dx(f->cr);
		y = (float)p.y / Dy(f->cr);
	}

	if(f->area->floating)
		trampoline(TFloat, f);
	else
		trampoline(THCol, f);

	if(grabmod)
		warppointer(addpt(f->cr.min, Pt(x * Dx(f->cr),
					       y * Dy(f->cr))));
	else
		warppointer(grabboxcenter(f));
}

static int
_openstack_down(Frame *f, int h) {
	int ret;
	int dy;

	if(f == nil)
		return 0;;
	ret = 0;
	if(!f->collapsed) {
		dy = Dy(f->colr) - labelh(def.font);
		if(dy >= h) {
			f->colr.min.y += h;
			return h;
		}else {
			f->collapsed = true;
			f->colr.min.y += dy;
			ret = dy;
			h -= dy;
		}
	}
	dy = _openstack_down(f->anext, h);
	f->colr.min.y += dy;
	f->colr.max.y += dy;
	return ret + dy;
}

static int
_openstack_up(Frame *f, int h) {
	int ret;
	int dy;

	if(f == nil)
		return 0;
	ret = 0;
	if(!f->collapsed) {
		dy = Dy(f->colr) - labelh(def.font);
		if(dy >= h) {
			f->colr.max.y -= h;
			return h;
		}else {
			f->collapsed = true;
			f->colr.max.y -= dy;
			ret = dy;
			h -= dy;
		}
	}
	dy = _openstack_up(f->aprev, h);
	f->colr.min.y -= dy;
	f->colr.max.y -= dy;
	return ret + dy;
}

static void
column_openstack(Area *a, Frame *f, int h) {

	if(f == nil)
		_openstack_down(a->frame, h);
	else {
		h -= _openstack_down(f->anext, h);
		if(h)
			_openstack_up(f->aprev, h);
	}
}

static void
column_drop(Area *a, Frame *f, int y) {
	Frame *ff;
	int dy;

	for(ff=a->frame; ff; ff=ff->anext)
		assert(ff != f);

	if(a->frame == nil || y <= a->frame->cr.min.y) {
		f->collapsed = true;
		f->colr.min.y = 0;
		f->colr.max.y = labelh(def.font);
		column_openstack(a, nil, labelh(def.font));
		column_insert(a, f, nil);
		return;
	}
	for(ff=a->frame; ff->anext; ff=ff->anext)
		if(y <= ff->colr.max.y) break;

	y = max(y, ff->colr.min.y + labelh(def.font));
	y = min(y, ff->colr.max.y);
	dy = ff->colr.max.y - y;
	if(dy <= labelh(def.font)) {
		f->collapsed = true;
		f->colr.min.y = 0;
		f->colr.max.y = labelh(def.font);
		column_openstack(a, ff, labelh(def.font) - dy);
	}else {
		f->colr.min.y = y;
		f->colr.max.y = ff->colr.max.y;
		ff->colr.max.y = y;
	}
	column_insert(a, f, ff);
}

static int
thcol(Frame *f) {
	Framewin *fw;
	Frame *fp, *fn;
	Area *a;
	Point pt, pt2;
	uint button;
	int ret, collapsed;

	focus(f->client, false);

	ret = TDone;
	if(!grabpointer(&scr.root, nil, cursor[CurIcon], MouseMask))
		return TDone;

	pt = querypointer(&scr.root);
	pt2.x = f->area->cr.min.x;
	pt2.y = pt.y;
	fw = framewin(f, pt2, OHoriz, Dx(f->area->cr));

	vplace(fw, pt);
	for(;;)
		switch (readmouse(&pt, &button)) {
		case MotionNotify:
			vplace(fw, pt);
			break;
		case ButtonRelease:
			if(button != 1)
				continue;
			SET(collapsed);
			SET(fp);
			SET(fn);
			a = f->area;
			if(a->floating)
				area_detach(f);
			else {
				collapsed = f->collapsed;
				fp = f->aprev;
				fn = f->anext;
				column_remove(f);
				if(!f->collapsed)
					if(fp)
						fp->colr.max.y = f->colr.max.y;
					else if(fn && fw->pt.y > fn->cr.min.y)
						fn->colr.min.y = f->colr.min.y;
			}

			column_drop(fw->ra, f, fw->pt.y);
			if(!a->floating && collapsed) {
				/* XXX */
				for(; fn && fn->collapsed; fn=fn->anext)
					;
				if(fn == nil)
					for(fn=fp; fn && fn->collapsed; fn=fn->aprev)
						;
				if(fp)
					fp->colr.max.x += labelh(def.font);
			}


			/* XXX: Multihead. */
 			if(!a->frame && !a->floating && f->view->areas->next)
 				area_destroy(a);

			frame_focus(f);
			goto done;
		case ButtonPress:
			if(button == 2)
				ret = TVCol;
			else if(button == 3)
				ret = TFloat;
			else
				continue;
			goto done;
		}
done:
	framedestroy(fw);
	return ret;
}

static int
tvcol(Frame *f) {
	Framewin *fw;
	Window *cwin;
	WinAttr wa;
	Rectangle r;
	Point pt, pt2;
	uint button;
	int ret;

	focus(f->client, false);

	pt = querypointer(&scr.root);
	pt2.x = pt.x;
	pt2.y = f->area->cr.min.y;

	r = view_rect(f->view);
	fw = framewin(f, pt2, OVert, Dy(r));

	r.min.y += fw->grabbox.min.y + Dy(fw->grabbox)/2;
	r.max.y = r.min.y + 1;
	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	ret = TDone;
	if(!grabpointer(&scr.root, cwin, cursor[CurIcon], MouseMask))
		goto done;

	hplace(fw, pt);
	for(;;)
		switch (readmouse(&pt, &button)) {
		case MotionNotify:
			hplace(fw, pt);
			continue;
		case ButtonPress:
			if(button == 2)
				ret = THCol;
			else if(button == 3)
				ret = TFloat;
			else
				continue;
			goto done;
		case ButtonRelease:
			if(button != 1)
				continue;
			if(fw->ra) {
				fw->ra = column_new(f->view, fw->ra, 0);
				area_moveto(fw->ra, f);
			}
			goto done;
		}

done:
	framedestroy(fw);
	destroywindow(cwin);
	return ret;
}

static int
tfloat(Frame *f) {
	Rectangle *rects;
	Rectangle frect, origin;
	Point pt, pt1;
	Client *c;
	Align align;
	uint nrect, button;
	int ret;

	c = f->client;
	if(!f->area->floating) {
		if(f->anext)
			f->anext->colr.min.y = f->colr.min.y;
		else if(f->aprev)
			f->aprev->colr.max.y = f->colr.max.y;
		area_moveto(f->view->floating, f);
	}
	map_frame(f->client);
	focus(f->client, false);

	ret = TDone;
	if(!grabpointer(c->framewin, nil, cursor[CurMove], MouseMask))
		return TDone;

	rects = view_rects(f->view, &nrect, f);
	origin = f->cr;
	frect = f->cr;

	pt = querypointer(&scr.root);
	pt1 = grabboxcenter(f);
	goto case_motion;

shut_up_ken:
	for(;;pt1=pt)
		switch (readmouse(&pt, &button)) {
		default: goto shut_up_ken;
		case MotionNotify:
		case_motion:
			origin = rectaddpt(origin, subpt(pt, pt1));
			origin = constrain(origin, -1);
			frect = origin;

			align = Center;
			snap_rect(rects, nrect, &frect, &align, def.snap);

			frect = frame_hints(f, frect, Center);
			frect = constrain(frect, -1);
			client_resize(c, frect);
			continue;
		case ButtonRelease:
			if(button != 1)
				continue;
			goto done;
		case ButtonPress:
			if(button != 3)
				continue;
			unmap_frame(f->client);
			ret = THCol;
			goto done;
		}
done:
	free(rects);
	return ret;
}

