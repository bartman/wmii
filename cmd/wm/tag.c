/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static Bool
istag(char **tags, unsigned int ntags, char *tag)
{
	unsigned int i;
	for(i = 0; i < ntags; i++)
		if(!strncmp(tags[i], tag, strlen(tag)))
			return True;
	return False;
}

Tag *
alloc_tag(char *name)
{
	static unsigned short id = 1;
    Tag *t = cext_emallocz(sizeof(Tag));

	t->id = id++;
	t->ntag = str2tags(t->tag, name);
	alloc_area(t);
	alloc_area(t);
	tag = (Tag **)cext_array_attach((void **)tag, t, sizeof(Tag *), &tagsz);
	ntag++;
	focus_tag(t);
    return t;
}

static void
destroy_tag(Tag *t)
{
	while(t->narea)
		destroy_area(t->area[0]);

	cext_array_detach((void **)tag, t, &tagsz);
	ntag--;
	if(sel >= ntag)
		sel = 0;

    free(t); 
}

int
tag2index(Tag *t)
{
	int i;
	for(i = 0; i < ntag; i++)
		if(t == tag[i])
			return i;
	return -1;
}

static void
update_frame_selectors(Tag *t)
{
	unsigned int i, j;

	/* select correct frames of clients */
	for(i = 0; i < nclient; i++)
		for(j = 0; j < client[i]->nframe; j++)
			if(client[i]->frame[j]->area->tag == t)
				client[i]->sel = j;
}

void
focus_tag(Tag *t)
{
	char buf[256];
	char name[256];
	unsigned int i;

	if(!ntag)
		return;

	XGrabServer(dpy);
	sel = tag2index(t);

	update_frame_selectors(t);

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < nclient; i++)
		if(client[i]->nframe) {
			Frame *f = client[i]->frame[client[i]->sel];
			if(f->area->tag == t) {
				XMoveWindow(dpy, client[i]->framewin, f->rect.x, f->rect.y);
				if(client[i]->nframe > 1)
					resize_client(client[i], &f->rect, False);
				draw_client(client[i]);
			}
			else
				XMoveWindow(dpy, client[i]->framewin, 2 * rect.width + f->rect.x, f->rect.y);
		}
	tags2str(name, sizeof(name), t->tag, t->ntag);
	snprintf(buf, sizeof(buf), "FocusTag %s\n", name);
	write_event(buf);
	XSync(dpy, False);
	XUngrabServer(dpy);
}

XRectangle *
rectangles(Tag *t, Bool isfloat, unsigned int *num)
{
    XRectangle *result = nil;
	unsigned int i;
	
	*num = 0;
	if(isfloat)
		*num = t->area[0]->nframe;
	else {
		for(i = 1; i < t->narea; i++)
			*num += t->area[i]->nframe;
	}

	if(*num) {
        result = cext_emallocz(*num * sizeof(XRectangle));
		if(isfloat) {
			for(i = 0; i < *num; i++)
				result[i] = t->area[0]->frame[0]->rect;
		}
		else {
			unsigned int j, n = 0;
			for(i = 1; i < t->narea; i++) {
				for(j = 0; j < t->area[i]->nframe; j++)
					result[n++] = t->area[i]->frame[j]->rect;
			}
		}
	}
    return result;
}

int
tid2index(unsigned short id)
{
	int i;
	for(i = 0; i < ntag; i++)
		if(tag[i]->id == id)
			return i;
	return -1;
}

Tag *
get_tag(char *name)
{
	unsigned int i, j, ntags;
	Tag *t = nil;
	char tname[256];
	char tags[MAX_TAGS][MAX_TAGLEN];

	for(i = 0; i < ntag; i++) {
		t = tag[i];
		tags2str(tname, sizeof(tname), t->tag, t->ntag);
		if(!strncmp(tname, name, strlen(name)))
			return t;
	}

	ntags = str2tags(tags, name);
	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client[i], tags[j]))
				goto Createtag;
	return nil;

Createtag:
	t = alloc_tag(name);
	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client[i], tags[j]) && !clientoftag(t, client[i]))
				attach_totag(t, client[i]);
	return t;
}

static Bool
hasclient(Tag *t)
{
	unsigned int i;
	for(i = 0; i < t->narea; i++)
		if(t->area[i]->nframe)
			return True;
	return False;
}

void
select_tag(char *arg)
{
	int i;
	Client *c;
	Tag *t = get_tag(arg);

	if(!t)
		return;
	cext_strlcpy(def.tag, arg, sizeof(def.tag));
	if(!istag(ctag, nctag, arg)) {
		char buf[256];
		ctag = (char **)cext_array_attach((void **)ctag, strdup(arg),
				sizeof(char *), &ctagsz);
		nctag++;
		snprintf(buf, sizeof(buf), "NewTag %s\n", arg);
		write_event(buf);
	}
    focus_tag(t);

	/* cleanup on select */
	for(i = 0; i < ntag; i++)
		if(!hasclient(tag[i])) {
			destroy_tag(tag[i]);
			i--;
		}

	if((c = sel_client_of_tag(t)))
		focus_client(c);
}

Bool
clientoftag(Tag *t, Client *c)
{
	unsigned int i;
	for(i = 0; i < t->narea; i++)
		if(clientofarea(t->area[i], c))
			return True;
	return False;
}

static void
organize_client(Tag *t, Client *c)
{
	unsigned int i;
	Bool hastag = False;
	for(i = 0; i < t->ntag; i++) {
		if(clienthastag(c, t->tag[i]))
			hastag = True;
		break;
	}

	if(hastag) {
		if(!clientoftag(t, c))
			attach_totag(t, c);
	}
	else {
		if(clientoftag(t, c))
			detach_fromtag(t, c);
	}
}

void
update_tags()
{
	unsigned int i, j;
	char buf[256];
	char **newctag = nil;
	unsigned int newctagsz = 0, nnewctag = 0;

	for(i = 0; i < nclient; i++) {
		for(j = 0; j < client[i]->ntag; j++) {
			if(!strncmp(client[i]->tag[j], "~", 2)) /* magic floating tag */
				continue;
			if(!istag(newctag, nnewctag, client[i]->tag[j])) {
				newctag = (char **)cext_array_attach((void **)newctag, strdup(client[i]->tag[j]),
							sizeof(char *), &newctagsz);
				nnewctag++;
			}
		}
	}

	for(i = 0; i < ntag; i++)
		if(hasclient(tag[i])) {
		   	tags2str(buf, sizeof(buf), tag[i]->tag, tag[i]->ntag);
			if(!istag(newctag, nnewctag, buf)) {
				newctag = (char **)cext_array_attach((void **)newctag, strdup(buf),
							sizeof(char *), &newctagsz);
				nnewctag++;
			}
		}

	/* propagate tagging events */
	for(i = 0; i < nnewctag; i++)
		if(!istag(ctag, nctag, newctag[i])) {
			snprintf(buf, sizeof(buf), "NewTag %s\n", newctag[i]);
			write_event(buf);
		}
	for(i = 0; i < nctag; i++) {
		if(!istag(newctag, nnewctag, ctag[i])) {
			snprintf(buf, sizeof(buf), "RemoveTag %s\n", ctag[i]);
			write_event(buf);
		}
		free(ctag[i]);
	}

	free(ctag);
	ctag = newctag;
	nctag = nnewctag;
	ctagsz = newctagsz;

	for(i = 0; i < nclient; i++) {
		for(j = 0; j < ntag; j++) {
			Tag *t = tag[j];
			if(j == sel)
				continue;
			organize_client(t, client[i]);
		}
		organize_client(tag[sel], client[i]);
	}

	if(!ntag && nctag)
		select_tag(ctag[0]);
}
 
void
detach_fromtag(Tag *t, Client *c)
{
	int i;
	Client *cl;
	for(i = 0; i < t->narea; i++) {
		if(clientofarea(t->area[i], c)) {
			detach_fromarea(t->area[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
	if((cl = sel_client_of_tag(t)))
		focus_client(cl);
}

void
attach_totag(Tag *t, Client *c)
{
	Area *a;

	if(c->trans || clienthastag(c, "~"))
		a = t->area[0];
	else
   		a = t->area[t->sel];

	attach_toarea(a, c);
    map_client(c);
	XMapWindow(dpy, c->framewin);
	if(t == tag[sel])
		focus_client(c);
}

Client *
sel_client_of_tag(Tag *t)
{
	if(t) {
		Area *a = t->narea ? t->area[t->sel] : nil;
		return (a && a->nframe) ? a->frame[a->sel]->client : nil;
	}
	return nil;
}

void
restack_tag(Tag *t)
{
	unsigned int i, j, n = 0;
	static Window *wins = nil;
   	static unsigned int winssz = 0;

	if(nclient > winssz) {
		winssz = 2 * nclient;
		free(wins);
		wins = cext_emallocz(sizeof(Window) * winssz);
	}

	for(i = 0; i < t->narea; i++) {
		Area *a = t->area[i];
		if(a->nframe) {
			wins[n++] = a->frame[a->sel]->client->framewin;
			for(j = 0; j < a->nframe; j++) {
				if(j == a->sel)
					continue;
				wins[n++] = a->frame[j]->client->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}

unsigned int
str2tags(char tags[MAX_TAGS][MAX_TAGLEN], const char *stags)
{
	unsigned int i, n;
	char buf[256];
	char *toks[MAX_TAGS];

	cext_strlcpy(buf, stags, sizeof(buf));
	n = cext_tokenize(toks, MAX_TAGS, buf, '+');
	for(i = 0; i < n; i++)
		cext_strlcpy(tags[i], toks[i], MAX_TAGLEN);
	return n;
}

void
tags2str(char *stags, unsigned int stagsz,
		 char tags[MAX_TAGS][MAX_TAGLEN], unsigned int ntags)
{
	unsigned int i, len = 0, l;

	stags[0] = 0;
	for(i = 0; i < ntags; i++) {
		l = strlen(tags[i]);
		if(len + l + 1 >= stagsz)
			return;
		if(len)
			stags[len++] = '+';
		memcpy(stags + len, tags[i], l);
		len += l;
		stags[len] = 0;
	}
}
