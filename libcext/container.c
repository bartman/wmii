/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "cext.h"

static int comp_ptr(void *p1, void *p2)
{
	return p1 == p2;
}

static void detach_from_stack(Container *c, CItem *i)
{	
	fprintf(stderr, "XX %s\n", "detach_from_stack()");
	/* remove from stack */
	if (i == c->stack) {
		c->stack = i->down;	
		if (c->stack)
			c->stack->up = 0;
	}
	else {
		if (i->up)
			i->up->down = i->down;
		if (i->down)
			i->down->up = i->up;
	}
	i->up = 0;
	i->down = 0;
}

static void attach_to_stack(Container *c, CItem *i)
{
	fprintf(stderr, "XX %s\n", "attach_to_stack()");
	i->up = 0;
	i->down = 0;
	if (!c->stack)
		c->stack = i;
	else {
		i->down = c->stack;
		c->stack->up = i;
		c->stack = i;
	}
}

void cext_attach_item(Container *c, void *item)
{
	CItem *i, *new = cext_emallocz(sizeof(CItem));
	fprintf(stderr, "XX %s\n", "cext_attach_item()");
	new->item = item;
	if (!c->list)
		c->list = new;
	else {
		for (i = c->list; i->next; i = i->next);
		i->next = new;
	}
	attach_to_stack(c, new);
}

void cext_detach_item(Container *c, void *item)
{
	CItem *i = c->list;
	fprintf(stderr, "XX %s\n", "cext_detach_item()");
	if (!i)
		return;

	/* remove from list */
	if (i->item == item)
		c->list = i->next;
	else {
		CItem *tmp;
		for (; i->next && (i->next->item != item); i = i->next);
		if (!i->next)
			return;
		tmp = i;
		i = i->next;
		tmp->next = tmp->next->next;
	}
	detach_from_stack(c, i);
	free(i);
}

static CItem *cext_find_citem(Container *c, void *pattern, int (*comp)(void *pattern, void *item))
{
	CItem *i;
	fprintf(stderr, "XX %s\n", "cext_find_citem()");
	for (i = c->list; i && !comp(pattern, i->item); i = i->next);
	return i;
}

void *cext_find_item(Container *c, void *pattern, int (*comp)(void *pattern, void *item))
{
	CItem *i = cext_find_citem(c, pattern, comp);
	fprintf(stderr, "XX %s\n", "cext_find_item()");
	return i ? i->item : nil;
}

void cext_iterate(Container *c, void *aux, void (*iter)(void *, void *aux))
{
	CItem *i;
	fprintf(stderr, "XX %s\n", "cext_iterate()");
	for (i = c->list; i; i = i->next)
	{
		assert(c);
		assert(i->item);
		iter(i->item, aux);
	}
}

size_t cext_sizeof(Container *c)
{
	size_t idx = 0;
	CItem *i;

	fprintf(stderr, "XX %s\n", "cext_sizeof()");
	for (i = c->list; i; i = i->next)
		idx++;

	return idx;
}

void cext_swap_items(Container *c, void *item1, void *item2)
{
	CItem *i1 = cext_find_citem(c, item1, comp_ptr);
	CItem *i2 = cext_find_citem(c, item2, comp_ptr);
	fprintf(stderr, "XX %s\n", "cext_swap_items()");
	
	i1->item = item2;
	i2->item = item1;
}

void cext_stack_top_item(Container *c, void *item)
{
	CItem *i = cext_find_citem(c, item, comp_ptr);
	fprintf(stderr, "XX %s\n", "cext_top_item()");
	if (!i)
		return;
	detach_from_stack(c, i);
	attach_to_stack(c, i);	
}

void *cext_stack_get_top_item(Container *c)
{
	fprintf(stderr, "XX %s\n", "cext_get_top_item()");
	return c->stack ? c->stack->item : nil;
}

void *cext_stack_get_down_item(Container *c, void *item)
{
	CItem *i = cext_find_citem(c, item, comp_ptr);
	fprintf(stderr, "XX %s\n", "cext_get_down_item()");
	if (!i)
		return nil;
	return i->down ? i->down->item : c->stack->item;
}

void *cext_stack_get_up_item(Container *c, void *item)
{
	CItem *i = cext_find_citem(c, item, comp_ptr);
	CItem *bottom;
	fprintf(stderr, "XX %s\n", "cext_get_up_item()");
	if (!i)
		return nil;
	for (bottom = c->stack; bottom && bottom->down; bottom = bottom->down);
	return i->up ? i->up->item : bottom->item;
}

void *cext_list_get_item(Container *c, size_t index)
{
	size_t idx = 0;
	CItem *i;

	fprintf(stderr, "XX %s\n", "cext_get_item()");
	for (i = c->list; i && index != idx; i = i->next)
		idx++;

	return i ? i->item : nil;
}

int cext_list_get_item_index(Container *c, void *item)
{
	int idx = 0;
	CItem *i;

	fprintf(stderr, "XX %s\n", "cext_get_item_index()");
	for (i = c->list; i && i->item != item; i = i->next)
		idx++;

	return i ? idx : -1;
}

void *cext_list_get_next_item(Container *c, void *item)
{
	size_t size = cext_sizeof(c);
	int idx = cext_list_get_item_index(c, item);
	if (idx == -1)
		return nil;

	if (idx + 1 < size)
		return cext_list_get_item(c, idx + 1);
	else
		return cext_list_get_item(c, 0);
}

void *cext_list_get_prev_item(Container *c, void *item)
{
	size_t size = cext_sizeof(c);
	int idx = cext_list_get_item_index(c, item);
	if (idx == -1)
		return nil;

	if (idx - 1 < 0)
		return cext_list_get_item(c, size - 1);
	else
		return cext_list_get_item(c, idx - 1);
}
