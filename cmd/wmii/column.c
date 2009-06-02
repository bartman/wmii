/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <strings.h>
#include "fns.h"

static void	column_resizeframe_h(Frame*, Rectangle);

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

bool
column_setmode(Area *a, const char *mode) {
	char *s, *t, *orig;
	char add, old;

	/* The mapping between the current internal
	 * representation and the external interface
	 * is currently a bit complex. That will probably
	 * change.
	 */

	orig = strdup(mode);
	t = orig;
	old = '\0';
	for(s=t; *s; s=t) {
		add = old;
		while((old=*s) && !strchr("+-^", old))
			s++;
		*s = '\0';
		if(s > t) {
			if(!strcmp(t, "max")) {
				if(add == '\0' || add == '+')
					a->max = true;
				else if(add == '-')
					a->max = false;
				else
					a->max = !a->max;
			}else
			if(!strcmp(t, "stack")) {
				if(add == '\0' || add == '+')
					a->mode = Colstack;
				else if(add == '-')
					a->mode = Coldefault;
				else
					a->mode = a->mode == Colstack ? Coldefault : Colstack;
			}else
			if(!strcmp(t, "default")) {
				if(add == '\0' || add == '+') {
					a->mode = Coldefault;
					column_arrange(a, true);
				}else if(add == '-')
					a->mode = Colstack;
				else
					a->mode = a->mode == Coldefault ? Colstack : Coldefault;
			}else {
				free(orig);
				return false;
			}
		}
		t = s;
		if(old)
			t++;
		
	}
	free(orig);
	return true;
}

char*
column_getmode(Area *a) {

	return sxprint("%s%cmax", a->mode == Colstack ? "stack" : "default",
				  a->max ? '+' : '-');
}

Area*
column_new(View *v, Area *pos, uint w) {
	Area *a;

	assert(!pos || !pos->floating);
	a = area_create(v, pos, w);
	return a;
#if 0
	if(!a)
		return nil;

	view_arrange(v);
	view_update(v);
#endif
}

void
column_insert(Area *a, Frame *f, Frame *pos) {

	f->area = a;
	f->client->floating = false;
	f->column = area_idx(a);
	frame_insert(f, pos);
	if(a->sel == nil)
		area_setsel(a, f);
}

/* Temporary. */
static void
stack_scale(Frame *first, int height) {
	Frame *f;
	Area *a;
	uint dy;
	int surplus;

	a = first->area;

	/*
	 * Will need something like this.
	column_fit(a, &ncol, &nuncol);
	*/

	dy = 0;
	for(f=first; f && !f->collapsed; f=f->anext)
		dy += Dy(f->colr);

	/* Distribute the surplus.
	 */
	surplus = height - dy;
	for(f=first; f && !f->collapsed; f=f->anext)
		f->colr.max.y += ((float)Dy(f->cr) / dy) * surplus;
}

static void
stack_info(Frame *f, Frame **firstp, Frame **lastp, int *dyp, int *nframep) {
	Frame *ft, *first, *last;
	int dy, nframe;

	nframe = 0;
	dy = 0;
	first = f;
	last = f;

	for(ft=f; ft && ft->collapsed; ft=ft->anext)
		;
	if(ft && ft != f) {
		f = ft;
		dy += Dy(f->colr);
	}
	for(ft=f; ft && !ft->collapsed; ft=ft->aprev) {
		first = ft;
		nframe++;
		dy += Dy(ft->colr);
	}
	for(ft=f->anext; ft && !ft->collapsed; ft=ft->anext) {
		if(first == nil)
			first = ft;
		last = ft;
		nframe++;
		dy += Dy(ft->colr);
	}
	if(nframep) *nframep = nframe;
	if(firstp) *firstp = first;
	if(lastp) *lastp = last;
	if(dyp) *dyp = dy;
}

int
stack_count(Frame *f, int *mp) {
	Frame *fp;
	int n, m;

	n = 0;
	for(fp=f->aprev; fp && fp->collapsed; fp=fp->aprev)
		n++;
	m = ++n;
	for(fp=f->anext; fp && fp->collapsed; fp=fp->anext)
		n++;
	if(mp) *mp = m;
	return n;
}

Frame*
stack_find(Area *a, Frame *f, int dir, bool stack) {
	Frame *fp;

	switch (dir) {
	default:
		die("not reached");
	case North:
		if(f)
			for(f=f->aprev; f && f->collapsed && stack; f=f->aprev)
				;
		else {
			f = nil;
			for(fp=a->frame; fp; fp=fp->anext)
				if(!fp->collapsed || !stack)
					f = fp;
		}
		break;
	case South:
		if(f)
			for(f=f->anext; f && f->collapsed && stack; f=f->anext)
				;
		else
			for(f=a->frame; f && f->collapsed && stack; f=f->anext)
				;
		break;
	}
	return f;
}

/* TODO: Move elsewhere. */
bool
find(Area **ap, Frame **fp, int dir, bool wrap, bool stack) {
	Rectangle r;
	Frame *f;
	Area *a;

	f = *fp;
	a = *ap;
	r = f ? f->cr : a->cr;

	if(dir == North || dir == South) {
		*fp = stack_find(a, f, dir, stack);
		if(*fp)
			return true;
		if (!a->floating)
			*ap = area_find(a->view, r, dir, wrap);
		if(!*ap)
			return false;
		*fp = stack_find(*ap, *fp, dir, stack);
		return *fp;
	}
	if(dir != East && dir != West)
		die("not reached");
	*ap = area_find(a->view, r, dir, wrap);
	if(!*ap)
		return false;
	*fp = ap[0]->sel;
	return true;
}

void
column_attach(Area *a, Frame *f) {
	Frame *first;
	int nframe, dy, h;

	f->colr = a->cr;

	if(a->sel) {
		stack_info(a->sel, &first, nil, &dy, &nframe);
		h = dy / (nframe+1);
		f->colr.max.y = f->colr.min.y + h;
		stack_scale(first, dy - h);
	}

	column_insert(a, f, a->sel);
	column_arrange(a, false);
}

void
column_detach(Frame *f) {
	Frame *first;
	Area *a;
	int dy;

	a = f->area;
	stack_info(f, &first, nil, &dy, nil);
	if(first && first == f)
		first = f->anext;
	column_remove(f);
	if(a->frame) {
		if(first)
			stack_scale(first, dy);
		column_arrange(a, false);
	}else if(a->view->areas->next)
		area_destroy(a);
}

static void column_scale(Area*);

void
column_attachrect(Area *a, Frame *f, Rectangle r) {
	Frame *fp, *pos;
	int before, after;

	pos = nil;
	for(fp=a->frame; fp; pos=fp, fp=fp->anext) {
		if(r.max.y < fp->cr.min.y)
			continue;
		before = fp->cr.min.y - r.min.y;
		after = r.max.y - fp->cr.max.y;
		if(abs(before) <= abs(after))
			break;
	}
	if(Dy(a->cr) > Dy(r)) {
		/* Kludge. */
		a->cr.max.y -= Dy(r);
		column_scale(a);
		a->cr.max.y += Dy(r);
	}
	column_insert(a, f, pos);
	column_scale(a);
	column_resizeframe_h(f, r);
}

void
column_remove(Frame *f) {
	Frame *pr;
	Area *a;

	a = f->area;
	pr = f->aprev;

	frame_remove(f);

	f->area = nil;
	if(a->sel == f) {
		if(pr == nil)
			pr = a->frame;
		if(pr && pr->collapsed)
			if(pr->anext && !pr->anext->collapsed)
				pr = pr->anext;
			else
				pr->collapsed = false;
		a->sel = nil;
		area_setsel(a, pr);
	}
}

static int
column_surplus(Area *a) {
	Frame *f;
	int surplus;

	surplus = Dy(a->cr);
	for(f=a->frame; f; f=f->anext)
		surplus -= Dy(f->cr);
	return surplus;
}

static void
column_fit(Area *a, uint *ncolp, uint *nuncolp) {
	Frame *f, **fp;
	uint minh, dy;
	uint ncol, nuncol;
	uint colh, uncolh;
	int surplus, i, j;

	/* The minimum heights of collapsed and uncollpsed frames.
	 */
	minh = labelh(def.font);
	colh = labelh(def.font);
	uncolh = minh + colh + 1;
	if(a->max && !resizing)
		colh = 0;

	/* Count collapsed and uncollapsed frames. */
	ncol = 0;
	nuncol = 0;
	for(f=a->frame; f; f=f->anext) {
		frame_resize(f, f->colr);
		if(f->collapsed)
			ncol++;
		else
			nuncol++;
	}

	if(nuncol == 0) {
		nuncol++;
		ncol--;
		(a->sel ? a->sel : a->frame)->collapsed = false;
	}

	/* FIXME: Kludge. */
	dy = Dy(view_rect(a->view)) - Dy(a->cr);
	minh = colh * (ncol + nuncol - 1) + uncolh;
	if(dy && Dy(a->cr) < minh)
		a->cr.max.y += min(dy, minh - Dy(a->cr));

	surplus = Dy(a->cr)
		- (ncol * colh)
		- (nuncol * uncolh);

	/* Collapse until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / (uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	/* Push to the floating layer until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		surplus += i * colh;
	}

	/* Decide which to collapse and which to float. */
	j = nuncol - 1;
	i = ncol - 1;
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f != a->sel) {
			if(!f->collapsed) {
				if(j < 0)
					f->collapsed = true;
				j--;
			}
			if(f->collapsed) {
				if(i < 0) {
					f->collapsed = false;
					area_moveto(f->view->floating, f);
					continue;
				}
				i--;
			}
		}
		/* Doesn't change if we 'continue' */
		fp = &f->anext;
	}

	if(ncolp) *ncolp = ncol;
	if(nuncolp) *nuncolp = nuncol;
}

void
column_settle(Area *a) {
	Frame *f;
	uint yoff, yoffcr;
	int surplus, nuncol, n;

	nuncol = 0;
	surplus = column_surplus(a);
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) nuncol++;

	if(nuncol == 0) {
		fprint(2, "%s: Badness: No uncollapsed frames, column %d, view %q\n",
				argv0, area_idx(a), a->view->name);
		return;
	}
	if(surplus < 0)
		fprint(2, "%s: Badness: surplus = %d in column_settle, column %d, view %q\n",
				argv0, surplus, area_idx(a), a->view->name);

	yoff = a->cr.min.y;
	yoffcr = yoff;
	n = surplus % nuncol;
	surplus /= nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->cr = rectsetorigin(f->cr, Pt(a->cr.min.x, yoff));
		f->colr = rectsetorigin(f->colr, Pt(a->cr.min.x, yoffcr));
		f->cr.min.x = a->cr.min.x;
		f->cr.max.x = a->cr.max.x;
		if(def.incmode == ISqueeze && !resizing)
		if(!f->collapsed) {
			f->cr.max.y += surplus;
			if(n-- > 0)
				f->cr.max.y++;
		}
		yoff = f->cr.max.y;
		yoffcr = f->colr.max.y;
	}
}

static int
foo(Frame *f) {
	WinHints h;
	int maxh;

	h = frame_gethints(f);
	maxh = 0;
	if(h.aspect.max.x)
		maxh = h.baspect.y +
		       (Dx(f->cr) - h.baspect.x) *
		       h.aspect.max.y / h.aspect.max.x;
	maxh = max(maxh, h.max.y);

	if(Dy(f->cr) >= maxh)
		return 0;
	return h.inc.y - (Dy(f->cr) - h.base.y) % h.inc.y;
}

static int
comp_frame(const void *a, const void *b) {
	int ia, ib;

	ia = foo(*(Frame**)a);
	ib = foo(*(Frame**)b);
	/* 
	 * I'd like to favor the selected client, but
	 * it causes windows to jump as focus changes.
	 */
	return ia < ib ? -1 :
	       ia > ib ?  1 :
	                  0;
}

static void
column_squeeze(Area *a) {
	static Vector_ptr fvec;
	Frame *f;
	int surplus, osurplus, dy, i;

	fvec.n = 0;
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) {
			f->cr = frame_hints(f, f->cr, 0);
			vector_ppush(&fvec, f);
		}

	surplus = column_surplus(a);
	for(osurplus=0; surplus != osurplus;) {
		osurplus = surplus;
		qsort(fvec.ary, fvec.n, sizeof *fvec.ary, comp_frame);
		for(i=0; i < fvec.n; i++) {
			f=fvec.ary[i];
			dy = foo(f);
			if(dy > surplus)
				break;
			surplus -= dy;
			f->cr.max.y += dy;
		}
	}
}

void
column_frob(Area *a) {
	Frame *f;

	for(f=a->frame; f; f=f->anext)
		f->cr = f->colr;
	column_settle(a);
	if(view_isselected(a->view))
		for(f=a->frame; f; f=f->anext)
			client_resize(f->client, f->cr);
}

static void
column_scale(Area *a) {
	Frame *f;
	uint dy;
	uint ncol, nuncol;
	uint colh;
	int surplus;

	if(!a->frame)
		return;

	column_fit(a, &ncol, &nuncol);

	colh = labelh(def.font);
	if(a->max && !resizing)
		colh = 0;

	dy = 0;
	surplus = Dy(a->cr);
	for(f=a->frame; f; f=f->anext) {
		if(f->collapsed)
			f->colr.max.y = f->colr.min.y + colh;
		else if(Dy(f->colr) == 0)
			f->colr.max.y++;
		surplus -= Dy(f->colr);
		if(!f->collapsed)
			dy += Dy(f->colr);
	}
	for(f=a->frame; f; f=f->anext) {
		f->dy = Dy(f->colr);
		f->colr.min.x = a->cr.min.x;
		f->colr.max.x = a->cr.max.x;
		if(!f->collapsed)
			f->colr.max.y += ((float)f->dy / dy) * surplus;
		if(btassert("6 full", !(f->collapsed ? Dy(f->cr) >= 0 : dy > 0)))
			warning("Something's fucked: %s:%d:%s()",
				__FILE__, __LINE__, __func__);
		frame_resize(f, f->colr);
	}

	if(def.incmode == ISqueeze && !resizing)
		column_squeeze(a);
	column_settle(a);
}

void
column_arrange(Area *a, bool dirty) {
	Frame *f;
	View *v;

	if(a->floating)
		float_arrange(a);
	if(a->floating || !a->frame)
		return;

	v = a->view;

	switch(a->mode) {
	case Coldefault:
		if(dirty)
			for(f=a->frame; f; f=f->anext)
				f->colr = Rect(0, 0, 100, 100);
		break;
	case Colstack:
		/* XXX */
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	default:
		fprint(2, "Dieing: %s: a: %p mode: %x floating: %d\n",
		       v->name, a, a->mode, a->floating);
		die("not reached");
		break;
	}
	column_scale(a);
	/* XXX */
	if(a->sel->collapsed)
		area_setsel(a, a->sel);
	if(view_isselected(v)) {
		//view_restack(v);
		client_resize(a->sel->client, a->sel->cr);
		for(f=a->frame; f; f=f->anext)
			client_resize(f->client, f->cr);
	}
}

void
column_resize(Area *a, int w) {
	Area *an;
	int dw;

	an = a->next;
	assert(an != nil);

	dw = w - Dx(a->cr);
	a->cr.max.x += dw;
	an->cr.min.x += dw;

	/* view_arrange(a->view); */
	view_update(a->view);
}

static void
column_resizeframe_h(Frame *f, Rectangle r) {
	Area *a;
	Frame *fn, *fp;
	uint minh;

	minh = labelh(def.font);

	a = f->area;
	fn = f->anext;
	fp = f->aprev;

	if(fp)
		r.min.y = max(r.min.y, fp->colr.min.y + minh);
	else /* XXX. */
		r.min.y = max(r.min.y, a->cr.min.y);

	if(fn)
		r.max.y = min(r.max.y, fn->colr.max.y - minh);
	else
		r.max.y = min(r.max.y, a->cr.max.y);

	if(fp) {
		fp->colr.max.y = r.min.y;
		frame_resize(fp, fp->colr);
	}
	if(fn) {
		fn->colr.min.y = r.max.y;
		frame_resize(fn, fn->colr);
	}

	f->colr = r;
	frame_resize(f, r);
}

void
column_resizeframe(Frame *f, Rectangle r) {
	Area *a, *al, *ar;
	View *v;
	uint minw;

	a = f->area;
	v = a->view;

	minw = Dx(view_rect(v)) / NCOL;

	al = a->prev;
	ar = a->next;

	if(al)
		r.min.x = max(r.min.x, al->cr.min.x + minw);
	else { /* Hm... */
		r.min.x = max(r.min.x, view_rect(v).min.x);
		r.max.x = max(r.max.x, r.min.x + minw);
	}

	if(ar)
		r.max.x = min(r.max.x, ar->cr.max.x - minw);
	else {
		r.max.x = min(r.max.x, view_rect(v).max.x);
		r.min.x = min(r.min.x, r.max.x - minw);
	}

	a->cr.min.x = r.min.x;
	a->cr.max.x = r.max.x;
	if(al) {
		al->cr.max.x = a->cr.min.x;
		column_arrange(al, false);
	}
	if(ar) {
		ar->cr.min.x = a->cr.max.x;
		column_arrange(ar, false);
	}

	column_resizeframe_h(f, r);

	/* view_arrange(v); */
	view_update(v);
}

