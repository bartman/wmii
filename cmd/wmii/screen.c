/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <stdlib.h>
#include "fns.h"

#ifdef notdef
void
mapscreens(void) {
	WMScreen *s, *ss;
	Rectangle r;
	int i, j;

#define frob(left, min, max, x, y) \
	if(Dy(r) > 0) /* If they intersect at some point on this axis */        \
	if(ss->r.min.x < s->r.min.x) {                                          \
		if((!s->left)                                                   \
		|| (abs(Dy(r)) < abs(s->left.max.x - s->min.x))) \
			s->left = ss;                                           \
	}

	/* Variable hell? Certainly. */
	for(i=0; i < nscreens; i++) {
		s = screens[i];
		for(j=0; j < nscreens; j++) {
			if(i == j)
				continue;
			ss = screens[j];
			r = rect_intersection(ss->r, s->r);
			frob(left,   min, max, x, y);
			frob(right,  max, min, x, y);
			frob(atop,   min, max, y, x);
			frob(below,  max, min, y, x);
		}
	}
#undef frob
}

int	findscreen(Rectangle, int);
int
findscreen(Rectangle rect, int direction) {
	Rectangle r;
	WMScreen *ss, *s;
	int best, i, j;

	best = -1;
#define frob(min, max, x, y)
	if(Dy(r) > 0) /* If they intersect at some point on this axis */
	if(ss->r.min.x < rect.min.x) {
		if(best == -1
		|| (abs(ss->r.max.x - rect.min.x) < abs(screens[best]->r.max.x - rect.min.x)))
			best = s->idx;
	}

	/* Variable hell? Certainly. */
	for(i=0; i < nscreens; i++) {
		ss = screens[j];
		r = rect_intersection(ss->r, rect);
		switch(direction) {
		default:
			return -1;
		case West:
			frob(min, max, x, y);
			break;
		case East:
			frob(max, min, x, y);
			break;
		case North:
			frob(min, max, y, x);
			break;
		case South:
			frob(max, min, y, x);
			break;
		}
	}
#undef frob
}
#endif

WMScreen *findscreen(Point pt)
{
	WMScreen *s, **sp;

	for(sp=screens; (s = *sp); sp++) {
		if (rect_haspoint_p(pt, s->r))
			return s;
	}

	return NULL;
}

WMScreen *findscreen_by_name(const char *name)
{
	WMScreen *s, **sp;

	if (!name || !strcmp(name, "sel"))
		return selscreen;

	for(sp=screens; (s = *sp); sp++) {
		if (!strcmp(name, s->name))
			return s;
	}

	return NULL;
}

static Rectangle
leastthing(Rectangle rect, int direction, Vector_ptr *vec, Rectangle (*key)(void*)) {
	Rectangle r;
	int i, best, d;

	SET(d);
	SET(best);
	for(i=0; i < vec->n; i++) {
		r = key(vec->ary[i]);
		switch(direction) {
		case South: d =  r.min.y; break;
		case North: d = -r.max.y; break;
		case East:  d =  r.min.x; break;
		case West:  d = -r.max.x; break;
		}
		if(i == 0 || d < best)
			best = d;
	}
	switch(direction) {
	case South: rect.min.y = rect.max.y =  best; break;
	case North: rect.min.y = rect.max.y = -best; break;
	case East:  rect.min.x = rect.max.x =  best; break;
	case West:  rect.min.x = rect.max.x = -best; break;
	}
	return rect;
}

void*
findthing(Rectangle rect, int direction, Vector_ptr *vec, Rectangle (*key)(void*), bool wrap) {
	Rectangle isect;
	Rectangle r, bestisect = {0,}, bestr = {0,};
	void *best, *p;
	int i, n;

	best = nil;

	/* For the record, I really hate these macros. */
#define frob(min, max, LT, x, y) \
	if(D##y(isect) > 0) /* If they intersect at some point on this axis */  \
	if(r.min.x LT rect.min.x) {                                             \
		n = abs(r.max.x - rect.min.x) - abs(bestr.max.x - rect.min.x);  \
		if(best == nil                                                  \
		|| n == 0 && D##y(isect) > D##y(bestisect)                      \
		|| n < 0                                                        \
		) {                                                             \
			best = p;                                               \
			bestr = r;                                              \
			bestisect = isect;                                      \
		}                                                               \
	}

	/* Variable hell? Certainly. */
	for(i=0; i < vec->n; i++) {
		p = vec->ary[i];
		r = key(p);
		isect = rect_intersection(rect, r);
		switch(direction) {
		default:
			die("not reached");
			/* Not reached */
		case West:
			frob(min, max, <, x, y);
			break;
		case East:
			frob(max, min, >, x, y);
			break;
		case North:
			frob(min, max, <, y, x);
			break;
		case South:
			frob(max, min, >, y, x);
			break;
		}
	}
#undef frob
	if(!best && wrap) {
		r = leastthing(rect, direction, vec, key);
		return findthing(r, direction, vec, key, false);
	}
	return best;
}

static int
area(Rectangle r) {
	return Dx(r) * Dy(r) *
	       (Dx(r) < 0 && Dy(r) < 0 ? -1 : 1);
}

int
ownerscreen(Rectangle r) {
	Rectangle isect;
	int s, a, best, besta;

	SET(besta);
	best = -1;
	for(s=0; s < nscreens; s++) {
		if(!screens[s]->showing)
			continue;
		isect = rect_intersection(r, screens[s]->r);
		a = area(isect);
		if(best < 0 || a > besta) {
			besta = a;
			best = s;
		}
	}
	return best;
}

View*
screen_selview(WMScreen *s) {
	if (s)
		return s->selview;
	return NULL;
}

void
screen_update(WMScreen *s) {
	View *v = s->selview;
	if(v)
		view_update(v);
}

void
screen_check_change(Window *w, XCrossingEvent *ev)
{
	Point pt = Pt(ev->x_root, ev->y_root);
	WMScreen *s;

	s = findscreen(pt);

	screen_change(s, CurrentArea);
}

void
screen_change(WMScreen *s, int select_area)
{
	View *v, *ov;
	Area *oa, *a;
	WMScreen *os;

	if (!s || s == selscreen)
		return;

	os = selscreen;
	ov = screen_selview(os);
	oa = ov ? ov->sel : NULL;

	event("UnfocusScreen %s\n", os->name);

	selscreen = s;
	event("FocusScreen %s\n", s->name);

	v = screen_selview(s);
	if (!v)
		return;

	event("FocusTag %s %s\n", v->name, s->name);

	a = NULL;
	if (oa && oa->floating && v->floating)
		a = v->floating;

	else
		switch(select_area) {
		case FirstArea:
			a = v->areas;
			break;
		case LastArea:
			if (v->areas)
				a = v->areas->prev;
			break;
		default:
			a = v->sel;
			break;
		}

	if (a)
		area_focus(a);

	if (v->sel)
		event("AreaFocus %a %s\n", v->sel, s->name);
}

