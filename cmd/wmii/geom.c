/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

Bool
ptinrect(Point pt, Rectangle r) {
	return (pt.x >= r.min.x) && (pt.x < r.max.x)
		&& (pt.y >= r.min.y) && (pt.y < r.max.y);
}

Align
quadrant(Rectangle r, Point pt) {
	Align ret = 0;

	pt = subpt(pt, r.min);

	if(pt.x >= Dx(r) * .5)
		ret |= EAST;
	if(pt.x <= Dx(r) * .5)
		ret |= WEST;
	if(pt.y <= Dy(r) * .5)
		ret |= NORTH;
	if(pt.y >= Dy(r) * .5)
		ret |= SOUTH;

	return ret;
}

Cursor
cursor_of_quad(Align align) {
	switch(align) {
	case NEAST:
		return cursor[CurNECorner];
	case NWEST:
		return cursor[CurNWCorner];
	case SEAST:
		return cursor[CurSECorner];
	case SWEST:
		return cursor[CurSWCorner];
	default:
		return cursor[CurMove];
	}
}

Align
get_sticky(Rectangle src, Rectangle dst) {
	Align stickycorner = 0;

	if(src.min.x != dst.min.x && src.max.x == dst.max.x)
		stickycorner |= EAST;
	else
		stickycorner |= WEST;
	if(src.min.y != dst.min.y && src.max.y == dst.max.y)
		stickycorner |= SOUTH;
	else    
		stickycorner |= NORTH;

	return stickycorner;
}
