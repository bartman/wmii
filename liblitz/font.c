/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <cext.h>

#include "blitz.h"

unsigned int
blitz_textwidth(BlitzFont *font, char *text)
{
	if(font->set) {
		XRectangle r;
		XmbTextExtents(font->set, text, strlen(text), nil, &r);
		return r.width;
	}
	return XTextWidth(font->xfont, text, strlen(text));
}

void
blitz_loadfont(BlitzFont *font, char *fontstr)
{
	char *fontname = fontstr;
	char **missing = nil, *def = "?";
	int n;

	setlocale(LC_ALL, "");
	if(font->set)
		XFreeFontSet(__blitz.display, font->set);
	font->set = XCreateFontSet(__blitz.display, fontname, &missing, &n, &def);
	if(missing) {
		while(n--)
			fprintf(stderr, "liblitz: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
		if(font->set) {
			XFreeFontSet(__blitz.display, font->set);
			font->set = nil;
		}
	}
	if(font->set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		unsigned int i;

		font->ascent = font->descent = 0;
		font_extents = XExtentsOfFontSet(font->set);
		n = XFontsOfFontSet(font->set, &xfonts, &font_names);
		for(i = 0, font->ascent = 0, font->descent = 0; i < n; i++) {
			if(font->ascent < (*xfonts)->ascent)
				font->ascent = (*xfonts)->ascent;
			if(font->descent < (*xfonts)->descent)
				font->descent = (*xfonts)->descent;
			xfonts++;
		}
	}
	else {
		if(font->xfont)
			XFreeFont(__blitz.display, font->xfont);
		font->xfont = nil;
		font->xfont = XLoadQueryFont(__blitz.display, fontname);
		if (!font->xfont) {
			fontname = "fixed";
			font->xfont = XLoadQueryFont(__blitz.display, fontname);
		}
		if (!font->xfont) {
			fprintf(stderr, "%s", "liblitz: error, cannot load 'fixed' font\n");
			exit(1);
		}
		font->ascent = font->xfont->ascent;
		font->descent = font->xfont->descent;
	}
}
