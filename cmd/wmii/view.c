/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static bool
empty_p(View *v) {
	Frame *f;
	Area *a;
	char **p;
	int cmp;

	foreach_frame(v, a, f) {
		cmp = 1;
		for(p=f->client->retags; *p; p++) {
			cmp = strcmp(*p, v->name);
			if(cmp >= 0)
				break;
		}
		if(cmp)
			return false;
	}
	return true;
}

static void
_view_select(WMScreen *s, View *v) {
	if(s->selview != v) {
		if(s->selview) {
			event("UnfocusTag %s %s\n",s->selview->name, s->name);
			s->selview->screen = NULL;
		}
		s->selview = v;
		v->screen = s;
		event("FocusTag %s %s\n", v->name, s->name);
		event("AreaFocus %a %s\n", v->sel, s->name);
		ewmh_updateview();
	}
}

Client*
view_selclient(View *v) {
	if(v && v->sel && v->sel->sel)
		return v->sel->sel->client;
	return nil;
}

bool
view_fullscreen_p(View *v) {
	Frame *f;

	for(f=v->floating->frame; f; f=f->anext)
		if(f->client->fullscreen >= 0)
			return true;
	return false;
}

View*
view_create(const char *name) {
	static ushort id = 1;
	View **vp;
	Client *c;
	View *v;
	int i;

	for(vp=&view; *vp; vp=&(*vp)->next) {
		i = strcmp((*vp)->name, name);
		if(i == 0)
			return *vp;
		if(i > 0)
			break;
	}

	v = emallocz(sizeof *v);
	v->id = id++;
	v->areas = emallocz(nscreens * sizeof *v->areas);

	utflcpy(v->name, name, sizeof v->name);

	event("CreateTag %s\n", v->name);
	area_create(v, nil, 0);

	view_init(v);
	
	area_focus(v->areas);

	v->next = *vp;
	*vp = v;

	/* FIXME: Belongs elsewhere */
	/* FIXME: Can do better. */
	for(c=client; c; c=c->next)
		if(c != kludge)
			apply_tags(c, c->tags);

	view_arrange(v);
	if(!selview())
		_view_select(selscreen, v);
	ewmh_updateviews();
	return v;
}

void
view_init(View *v) {
	v->areas = nil;
	column_new(v, nil, 0);
}

void
view_destroy(View *v) {
	View **vp;
	Frame *f;
	View *tv;
	Area *a;
	WMScreen *scrn;

	if (!empty_p(v))
		return;

	if(v->dead)
		return;
	v->dead = true;

	for(vp=&view; *vp; vp=&(*vp)->next)
		if(*vp == v) break;
	*vp = v->next;
	assert(v != v->next);

	/* Detach frames held here by regex tags. */
	/* FIXME: Can do better. */
	foreach_frame(v, a, f)
		apply_tags(f->client, f->client->tags);

	foreach_area(v, a)
		area_destroy(a);

	event("DestroyTag %s\n", v->name);

	scrn = view_which_screen(v);
	if(scrn) {
		for(tv=view; tv; tv=tv->next)
			if(tv->next == *vp) break;
		if(tv == nil)
			tv = view;
		if(tv)
			view_focus(scrn, tv);
	}
	free(v->areas);
	free(v);
	ewmh_updateviews();
}

Area*
view_findarea(View *v, int idx, bool create) {
	Area *a;

	for(a=v->areas; a && --idx > 0; a=a->next)
		if(create && a->next == nil)
			return area_create(v, a, 0);
	return a;
}

static void
frames_update_sel(View *v) {
	Frame *f;
	Area *a;

	if (!v)
		return;

	foreach_frame(v, a, f)
		f->client->sel = f;
}

/* Don't let increment hints take up more than half
 * of the screen, in either direction.
 */
static Rectangle
fix_rect(Rectangle old, Rectangle new) {
	double r;

	new = rect_intersection(new, old);

	r = (Dy(old) - Dy(new)) / Dy(old);
	if(r > .5) {
		r -= .5;
		new.min.y -= r * (new.min.y - old.min.y);
		new.max.y += r * (old.max.y - new.max.y);
	}
	r = (Dx(old) - Dx(new)) / Dx(old);
	if(r > .5) {
		r -= .5;
		new.min.x -= r * (new.min.x - old.min.x);
		new.max.x += r * (old.max.x - new.max.x);
	}
	return new;
}

void
view_update_rect(View *v) {
	static Vector_rect vec;
	static Vector_rect *vp;
	Rectangle r, sr, rr, brect, scrnr;
	WMScreen *scrn;
	Strut *strut;
	Frame *f;
	int s, i;
	/* These short variable names are hell, eh? */

	/* XXX:
	if(v != selview)
		return false;
	*/
	vec.n = 0;
	for(f=v->floating->frame; f; f=f->anext) {
		strut = f->client->strut;
		if(!strut)
			continue;
		vector_rpush(&vec, strut->top);
		vector_rpush(&vec, strut->left);
		vector_rpush(&vec, rectaddpt(strut->right, Pt(scr.rect.max.x, 0)));
		vector_rpush(&vec, rectaddpt(strut->bottom, Pt(0, scr.rect.max.y)));
	}
	/* Find the largest screen space not occupied by struts. */
	vp = unique_rects(&vec, scr.rect);
	scrnr = max_rect(vp);

	/* FIXME: Multihead. */
	scrn = view_which_screen(v);
	if (scrn)
		v->floating->cr = scrn->r;
	else
		v->floating->cr = scr.rect;

	for(s=0; s < nscreens; s++) {
		scrn = screens[s];
		r = fix_rect(scrn->r, scrnr);

		/* Ugly. Very, very ugly. */
		/*
		 * Try to find some rectangle near the edge of the
		 * screen where the bar will fit. This way, for
		 * instance, a system tray can be placed there
		 * without taking up too much extra screen real
		 * estate.
		 */
		rr = r;
		brect = scrn->brect;
		for(i=0; i < vp->n; i++) {
			sr = rect_intersection(vp->ary[i], scrn->r);
			if(Dx(sr) < Dx(r)/2 || Dy(sr) < Dy(brect))
				continue;
			if(scrn->barpos == BTop && sr.min.y < rr.min.y
			|| scrn->barpos != BTop && sr.max.y > rr.max.y)
				rr = sr;
		}
		if(scrn->barpos == BTop) {
			bar_sety(scrn, rr.min.y);
			r.min.y = max(r.min.y, scrn->brect.max.y);
		}else {
			bar_sety(scrn, rr.max.y - Dy(brect));
			r.max.y = min(r.max.y, scrn->brect.min.y);
		}
		bar_setbounds(scrn, rr.min.x, rr.max.x);
		scrn->vrect = r;
	}
}

void
view_update(View *v) {
	Client *c;
	Frame *f;
	Area *a;

	if(starting || !view_isvisible(v))
		return;

	frames_update_sel(v);

	foreach_frame(v, a, f)
		if(f->client->fullscreen >= 0) {
			f->collapsed = false;
			if(!f->area->floating) {
				f->oldarea = area_idx(f->area);
				area_moveto(v->floating, f);
				area_setsel(v->floating, f);
			}else if(f->oldarea == -1)
				f->oldarea = 0;
		}

	view_arrange(v);

	for(c=client; c; c=c->next) {
		f = c->sel;
		if((f && f->view == v)
		&& (f->area == v->sel || !(f->area && f->area->max && f->area->floating))) {
			if(f->area)
				client_resize(c, f->cr);
		}else {
			if (!(f && view_isvisible(f->view))) {
				unmap_frame(c);
				client_unmap(c, IconicState);
			}
		}
		ewmh_updatestate(c);
		ewmh_updateclient(c);
	}

	view_restack(v);
	if(!v->sel->floating && view_fullscreen_p(v))
		area_focus(v->floating);
	else
		area_focus(v->sel);
	frame_draw_all();
}

void
view_focus(WMScreen *s, View *v) {
	
	USED(s);

	if (!s || !v)
		return;

	selscreen = s;
	_view_select(s, v);
	view_update(v);
}

void
view_select(const char *arg) {
	char buf[256];
	char *tag = NULL;
	char *scrn = NULL;
	WMScreen *s;

	utflcpy(buf, arg, sizeof buf);

	tag = strtok(buf, " \t");
	trim(tag, " \t+/");

	scrn = strtok(NULL, " \t");
	if (scrn)
		trim(scrn, " \t+/");

	if(tag[0] == '\0')
		return;
	if(!strcmp(tag, ".") || !strcmp(tag, ".."))
		return;

	s = findscreen_by_name(scrn);

	view_select_on(s, buf);
}

void
view_select_on(WMScreen *s, const char *name)
{
	View *v;

	v = view_create(name);
	if (view_isvisible(v))
		return;

	_view_select(s, v);
	view_update_all(s); /* performs view_focus */
}

void
view_attach(View *v, Frame *f) {
	Client *c;
	Frame *ff;
	Area *a, *oldsel;
	
	c = f->client;

	oldsel = v->oldsel;
	a = v->sel;
	if(client_floats_p(c)) {
		if(v->sel != v->floating && c->fullscreen < 0)
			oldsel = v->sel;
		a = v->floating;
	}
	else if((ff = client_groupframe(c, v)))
		a = ff->area;
	else if(v->sel->floating) {
		if(v->oldsel)
			a = v->oldsel;
		/* Don't float a frame when starting or when its
		 * last focused frame didn't float. Important when
		 * tagging with +foo.
		 */
		else if(starting
		     || c->sel && c->sel->area && !c->sel->area->floating)
			a = v->areas;
	}
	if(!a->floating && view_fullscreen_p(v))
		a = v->floating;

	area_attach(a, f);
	/* TODO: Decide whether to focus this frame */
	bool newgroup = !c->group
		     || c->group->ref == 1
		     || view_selclient(v)
		        && view_selclient(v)->group == c->group
		     || group_leader(c->group)
		        && !client_viewframe(group_leader(c->group),
					     c->sel->view);

	if(!(c->w.ewmh.type & (TypeSplash|TypeDock))) {
		if(newgroup
		&& !(c->tagre.regex && regexec(c->tagre.regc, v->name, nil, 0)))
			frame_focus(f);
		else if(c->group && f->area->sel->client->group == c->group)
			/* XXX: Stack. */
			area_setsel(f->area, f);
	}

	if(oldsel)
		v->oldsel = oldsel;

	if(c->sel == nil)
		c->sel = f;
	view_update(v);
}

void
view_detach(Frame *f) {
	Client *c;
	View *v;

	v = f->view;
	c = f->client;

	area_detach(f);
	if(c->sel == f)
		c->sel = f->cnext;

	if(view_isvisible(v))
		view_update(v);
	else if(empty_p(v))
		view_destroy(v);
}

char**
view_names(void) {
	Vector_ptr vec;
	View *v;

	vector_pinit(&vec);
	for(v=view; v; v=v->next)
		vector_ppush(&vec, v->name);
	vector_ppush(&vec, nil);
	return erealloc(vec.ary, vec.n * sizeof *vec.ary);
}

void
view_restack(View *v) {
	static Vector_long wins;
	Divide *d;
	Frame *f;
	Area *a;
	
	if(view_isvisible(v))
		return;

	wins.n = 0;

	/* *sigh */
	for(f=v->floating->stack; f; f=f->snext)
		if(f->client->w.ewmh.type & TypeDock)
			vector_lpush(&wins, f->client->framewin->w);
		else
			break;

	for(; f; f=f->snext)
		vector_lpush(&wins, f->client->framewin->w);

	for(int s=0; s < nscreens; s++)
		vector_lpush(&wins, screens[s]->barwin->w);

	for(d = divs; d && d->w->mapped; d = d->next)
		vector_lpush(&wins, d->w->w);

	foreach_column(v, a)
		if(a->frame) {
			vector_lpush(&wins, a->sel->client->framewin->w);
			for(f=a->frame; f; f=f->anext)
				if(f != a->sel)
					vector_lpush(&wins, f->client->framewin->w);
		}

	ewmh_updatestacking();
	if(wins.n)
		XRestackWindows(display, (ulong*)wins.ary, wins.n);
}

void
view_scale(View *v, int width) {
	uint xoff, numcol;
	uint minwidth;
	Area *a;
	float scale;
	int dx;

	minwidth = Dx(view_rect(v))/NCOL; /* XXX: Multihead. */

	if(!v->areas)
		return;

	numcol = 0;
	dx = 0;
	for(a=v->areas; a; a=a->next) {
		numcol++;
		dx += Dx(a->cr);
	}

	scale = (float)width / dx;
	xoff = view_rect(v).min.x;
	for(a=v->areas; a; a=a->next) {
		a->cr.max.x = xoff + Dx(a->cr) * scale;
		a->cr.min.x = xoff;
		if(!a->next)
			a->cr.max.x = view_rect(v).min.x + width;
		xoff = a->cr.max.x;
	}

	if(numcol * minwidth > width)
		return;

	xoff = view_rect(v).min.x;
	for(a=v->areas; a; a=a->next) {
		a->cr.min.x = xoff;

		if(Dx(a->cr) < minwidth)
			a->cr.max.x = xoff + minwidth;
		if(!a->next)
			a->cr.max.x = view_rect(v).min.x + width;
		xoff = a->cr.max.x;
	}
}

/* XXX: Multihead. */
void
view_arrange(View *v) {
	Area *a;

	if(!v || !v->areas)
		return;

	view_update_rect(v);
	view_scale(v, Dx(view_rect(v)));
	foreach_area(v, a) {
		if(a->floating)
			continue;
		/* This is wrong... */
		a->cr.min.y = view_rect(v).min.y;
		a->cr.max.y = view_rect(v).max.y;
		column_arrange(a, false);
	}
	if(view_isvisible(v))
		div_update_all();
}

Rectangle*
view_rects(View *v, uint *num, Frame *ignore) {
	Vector_rect result;
	Frame *f;
	int i;

	vector_rinit(&result);

	for(f=v->floating->frame; f; f=f->anext)
		if(f != ignore)
			vector_rpush(&result, f->cr);
	for(i=0; i < nscreens; i++) {
		vector_rpush(&result, view_rect(v));
		vector_rpush(&result, screens[i]->r);
	}

	*num = result.n;
	return result.ary;
}

void
view_update_all(WMScreen *s) {
	View *n, *v, *old;

	old = screen_selview(s);
	for(v=view; v; v=v->next)
		frames_update_sel(v);

	for(v=view; v; v=n) {
		n=v->next;
		if(view_isvisible(v) && v->screen != selscreen)
			continue;
		if(v != old && empty_p(v))
			view_destroy(v);
	}

	view_update(s->selview);
}

uint
view_newcolwidth(View *v, int num) {
	Rule *r;
	char *toks[16];
	char buf[sizeof r->value];
	ulong n;

	for(r=def.colrules.rule; r; r=r->next)
		if(regexec(r->regex, v->name, nil, 0)) {
			utflcpy(buf, r->value, sizeof buf);
			n = tokenize(toks, 16, buf, '+');
			if(num < n)
				if(getulong(toks[num], &n))
					return Dx(view_rect(v)) * (n / 100.0); /* XXX: Multihead. */
			break;
		}
	return 0;
}

char*
view_index(View *v) {
	Rectangle *r;
	Frame *f;
	Area *a;
	int i;

	bufclear();
	i = 0;
	foreach_area(v, a) {
		i++;
		if(a->floating)
			bufprint("# ~ %d %d\n", Dx(a->cr), Dy(a->cr));
		else
			bufprint("# %d %d %d\n", i, a->cr.min.x, Dx(a->cr));

		for(f=a->frame; f; f=f->anext) {
			r = &f->cr;
			if(a->floating)
				bufprint("~ %C %d %d %d %d %s\n",
						f->client,
						r->min.x, r->min.y,
						Dx(*r), Dy(*r),
						f->client->props);
			else
				bufprint("%d %C %d %d %s\n",
						i, f->client,
						r->min.y, Dy(*r),
						f->client->props);
		}
	}
	return buffer;
}

WMScreen*
view_which_screen(View *v)
{
	int s;
	WMScreen *scrn;

	if (!v)
		return NULL;

	if (v->screen)
		return v->screen;

#if 0
	for(s=0; s < nscreens; s++) {
		scrn = screens[s];

		if(v == screen_selview(scrn))
			return scrn;
	}
#endif

	return NULL;
}

bool
view_isvisible(View *v)
{
	return (bool)view_which_screen(v);
}

bool
view_isselected(View *v)
{
	return v == screen_selview(selscreen);
}

Rectangle
view_rect(View *v)
{
	if (v->screen)
		return v->screen->vrect;
	/* TODO: this avoids crashing by returning NON-NULL when v->screen is
	 * not assigned... to be fixed when we start using late scaling */
	return selscreen->vrect;
}

