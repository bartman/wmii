#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>
#include <ixp.h>
#include <x11.h>

#define BLOCK(x) do { x; }while(0)

#ifndef EXTERN
# define EXTERN extern
#endif

typedef struct Item	Item;

struct Item {
	char*	string;
	char*	retstring;
	Item*	next_link;
	Item*	next;
	Item*	prev;
	int	len;
	int	width;
};

EXTERN long	xtime;
EXTERN Image*	ibuf;
EXTERN Font*	font;
EXTERN CTuple	cnorm, csel;
EXTERN bool	ontop;

EXTERN Cursor	cursor[1];
EXTERN Visual*	render_visual;

EXTERN IxpServer	srv;

EXTERN Item*	items;
EXTERN Item*	matchfirst;
EXTERN Item*	matchstart;
EXTERN Item*	matchend;
EXTERN Item*	matchidx;

EXTERN Item*	histidx;

EXTERN char	filter[1024];

EXTERN int	maxwidth;
EXTERN int	result;

EXTERN char	buffer[8092];
EXTERN char*	_buffer;

static char*	const _buf_end = buffer + sizeof buffer;

#define bufclear() \
	BLOCK( _buffer = buffer; _buffer[0] = '\0' )
#define bufprint(...) \
	_buffer = seprint(_buffer, _buf_end, __VA_ARGS__)

