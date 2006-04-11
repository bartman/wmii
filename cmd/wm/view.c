/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Vector *
view2vector(ViewVector *vv)
{
	return (Vector *) vv;
}

View *
alloc_view(char *name)
{
	static unsigned short id = 1;
	View *v = cext_emallocz(sizeof(View));

	v->id = id++;
	cext_strlcpy(v->name, name, sizeof(v->name));
	alloc_area(v);
	alloc_area(v);
	sel = view.size;
	cext_vattach(view2vector(&view), v);
	return v;
}

void
destroy_view(View *v)
{
	fprintf(stderr, "destroy_view: %s\n", v->name);
	while(v->area.size)
		destroy_area(v->area.data[0]);

	cext_vdetach(view2vector(&view), v);
	if(sel >= view.size)
		sel = 0;

	free(v);
}

int
view2index(View *v)
{
	int i;
	for(i = 0; i < view.size; i++)
		if(v == view.data[i])
			return i;
	return -1;
}

static void
update_frame_selectors(View *v)
{
	unsigned int i, j;

	/* select correct frames of clients */
	for(i = 0; i < client.size; i++)
		for(j = 0; j < client.data[i]->frame.size; j++)
			if(client.data[i]->frame.data[j]->area->view == v)
				client.data[i]->sel = j;
}

void
focus_view(View *v)
{
	Client *c;
	unsigned int i;

	XGrabServer(dpy);
	sel = view2index(v);

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	if((c = sel_client_of_view(v)))
		focus_client(c);
	for(i = 0; i < client.size; i++)
		if(client.data[i]->frame.size) {
			Frame *f = client.data[i]->frame.data[client.data[i]->sel];
			if(f->area->view == v) {
				XMoveWindow(dpy, client.data[i]->framewin, f->rect.x, f->rect.y);
				if(client.data[i]->frame.size > 1)
					resize_client(client.data[i], &f->rect, False);
				draw_client(client.data[i]);
			}
			else
				XMoveWindow(dpy, client.data[i]->framewin,
						2 * rect.width + f->rect.x, f->rect.y);
		}
	update_bar_tags();
	XSync(dpy, False);
	XUngrabServer(dpy);
}

XRectangle *
rectangles(View *v, Bool isfloat, unsigned int *num)
{
	XRectangle *result = nil;
	unsigned int i;

	*num = 0;
	if(isfloat)
		*num = v->area.data[0]->frame.size;
	else {
		for(i = 1; i < v->area.size; i++)
			*num += v->area.data[i]->frame.size;
	}

	if(*num) {
		result = cext_emallocz(*num * sizeof(XRectangle));
		if(isfloat) {
			for(i = 0; i < *num; i++)
				result[i] = v->area.data[0]->frame.data[0]->rect;
		}
		else {
			unsigned int j, n = 0;
			for(i = 1; i < v->area.size; i++) {
				for(j = 0; j < v->area.data[i]->frame.size; j++)
					result[n++] = v->area.data[i]->frame.data[j]->rect;
			}
		}
	}
	return result;
}

int
vid2index(unsigned short id)
{
	int i;
	for(i = 0; i < view.size; i++)
		if(view.data[i]->id == id)
			return i;
	return -1;
}

View *
name2view(char *name)
{
	unsigned int i;

	for(i = 0; i < view.size; i++)
		if(!strncmp(view.data[i]->name, name, strlen(name)))
			return view.data[i];
	return nil;
}

View *
get_view(char *name)
{
	View *v = name2view(name);
	return v ? v : alloc_view(name);
}

static Bool
isempty(View *v)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(v->area.data[i]->frame.size)
			return False;
	return True;
}

void
select_view(char *arg)
{
	View *v = name2view(arg);
	if(!v)
		return;
	focus_view(v);
}

static Bool
isclientof(View *v, Client *c)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(clientofarea(v->area.data[i], c))
			return True;
	return False;
}

void
detach_fromview(View *v, Client *c)
{
	unsigned int i;

	for(i = 0; i < v->area.size; i++) {
		if(clientofarea(v->area.data[i], c)) {
			detach_fromarea(v->area.data[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
}

void
attach_toview(View *v, Client *c)
{
	Area *a;

	if(isclientof(v, c))
		return;

	if(c->trans || c->floating)
		a = v->area.data[0];
	else
		a = v->area.data[v->sel];

	attach_toarea(a, c);
	map_client(c);
	XMapWindow(dpy, c->framewin);
}

Client *
sel_client_of_view(View *v)
{
	if(v) {
		Area *a = v->area.size ? v->area.data[v->sel] : nil;
		return (a && a->frame.size) ? a->frame.data[a->sel]->client : nil;
	}
	return nil;
}

void
restack_view(View *v)
{
	unsigned int i, n = 0;
	int j;
	static Window *wins = nil;
	static unsigned int winssz = 0;

	if(client.size > winssz) {
		winssz = 2 * client.size;
		wins = realloc(wins, sizeof(Window) * winssz);
	}

	for(i = 0; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(a->frame.size) {
			wins[n++] = a->frame.data[a->sel]->client->framewin;
			for(j = a->frame.size - 1; j >= 0; j--) {
				Client *c = a->frame.data[j]->client;
				if((v->sel == i) && (a->sel == j)) {
					ungrab_mouse(c->framewin, AnyModifier, AnyButton);
					grab_mouse(c->framewin, Mod1Mask, Button1);
					grab_mouse(c->framewin, Mod1Mask, Button3);
				}
				else
					grab_mouse(c->framewin, AnyModifier, Button1);
				if(j == a->sel)
					continue;
				wins[n++] = c->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}

void
arrange_view(View *v, Bool dirty)
{
	unsigned int i, xoff = 0;
	unsigned int dx = 0;
	float scale = 1.0;

	if(v->area.size == 1)
		return;

	if(dirty) {
		for(i = 1; i < v->area.size; i++)
			dx += v->area.data[i]->rect.width;
		scale = (float)rect.width / (float)dx;
	}
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(dirty) {
			a->rect.x = xoff;
			a->rect.y = 0;
			a->rect.height = rect.height - brect.height;
			a->rect.width *= scale;
			xoff += a->rect.width;
		}
		arrange_column(a, False);
	}
}

static void
update_client_views(Client *c)
{
	char buf[256];
	char *toks[16];
	unsigned int i, n;

	cext_strlcpy(buf, c->tags, sizeof(buf));
	n = cext_tokenize(toks, 16, buf, '+');

	while(c->view.size)
		cext_vdetach(view2vector(&c->view), c->view.data[0]);

	for(i = 0; i < n; i++) {
		if(!strncmp(toks[i], "*", 2))
			continue;
		cext_vattach(view2vector(&c->view), get_view(toks[i]));
	}
}

static Bool
isviewof(Client *c, View *v)
{
	unsigned int i;
	for(i = 0; i < c->view.size; i++)
		if(c->view.data[i] == v)
			return True;
	return False;
}

static View *
emptyview()
{
	unsigned int i;
	for(i = 0; i < view.size; i++)
		if(isempty(view.data[i]))
			return view.data[i];
	return nil;
}

void
update_views()
{
	unsigned int i, j;
	View *v, *old = view.size ? view.data[sel] : nil;

	for(i = 0; i < client.size; i++)
		update_client_views(client.data[i]);

	for(i = 0; i < client.size; i++) {
		Client *c = client.data[i];
		for(j = 0; j < view.size; j++) {
			if(strstr(c->tags, "*"))
				attach_toview(view.data[j], c);
			else if(isviewof(c, view.data[j])) {
				if(!isclientof(view.data[j], c))
					attach_toview(view.data[j], c);
			}
			else {
				if(isclientof(view.data[j], c))
					detach_fromview(view.data[j], c);
			}
		}
	}

	while((v = emptyview())) {
		if(v == old)
			old = nil;
		destroy_view(v);
	}

	if(old)
		focus_view(old);
	else if(view.size)
		focus_view(view.data[sel]);
	else
		update_bar_tags();
}
