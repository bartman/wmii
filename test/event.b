implement Event;

include "sys.m";
	sys: Sys;
	OREAD, OWRITE: import Sys;
	print, sprint, read, open: import sys;
include "draw.m";
include "string.m";
	str: String;
include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;
include "lists.m";
	lists: Lists;
	append, reverse: import lists;
include "regex.m";
	regex: Regex;
	Re: import regex;
include "sh.m";
	sh: Sh;

Event: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

line: chan of string;

suicide()
{
	fd := open(sprint("/proc/%d/pgrp", sys->pctl(0, nil)), OWRITE);
	sys->fprint(fd, "kill");
}

buflines(in, out: chan of string)
{
	lines: list of string;
	for(;;) {
		if(lines == nil)
			lines = <-in :: nil;
		alt {
		l := <-in =>
			lines = append(lines, l);
		out <-= hd lines =>
			if(hd lines == nil)
				suicide();
			lines = tl lines;
		}
	}
}

readlines(c: chan of string, fd: ref sys->FD)
{
	out := chan of string;

	spawn buflines(out, c);

	b := bufio->fopen(fd, OREAD);
	while((s := b.gets('\n')) != nil)
		out <-= s;
	out <-= nil;
}

readfile(file: string): (string, int)
{
	fd := open(file, OREAD);
	if(fd == nil)
		return ("", 0);
	
	ret := "";
	buf := array[512] of byte;
	while((n := read(fd, buf, len buf)) > 0)
		ret += string buf[:n];
	return (ret, 1);
}

ishex(s: string): int
{
	if(len s < 3 || s[0:2] != "0x")
		return 0;
	s = s[2:];
	(nil, end) := str->toint(s, 16);
	return end == nil;
}

init(draw: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	str = load String String->PATH;
	bufio = load Bufio Bufio->PATH;
	lists = load Lists "/dis/lib/lists.dis";
	regex = load Regex Regex->PATH;
	sh = load Sh Sh->PATH;

	sys->pctl(sys->NEWPGRP, nil);

	sh->system(draw, "mount -A {os rc -c 'exec dial $WMII_ADDRESS' >[1=0]} /mnt/wmii &");

	line = chan of string;
	spawn readlines(line, sys->fildes(0));

	relist: list of ref (Re, int);

	argv = tl argv;
	for(; argv != nil; argv = tl argv) {
		vflag := 0;
		if(hd argv == "-v") {
			argv = tl argv;
			vflag = 1;
		}
		(re, err) := regex->compile(hd argv, 0);
		if(err != nil)
			raise sprint("bad regex %q: %s", hd argv, err);
		relist = ref (re, vflag) :: relist;
	}

	relist = reverse(relist);
	
line:	for(;;) {
		lin := <-line;
		if(lin == nil)
			break;
		l := str->unquoted(lin);
		if(l == nil)
			continue;

		(evn, end) := str->toint(hd l, 10);
		if(end == nil) {
			for(rel := relist; rel != nil; rel = tl relist) {
				(re, vflag) := *(hd rel);
				match := regex->execute(re, lin);
				if((match == nil) != vflag)
					continue line;
			}
			print("%s", lin);
			for(; l != nil; l = tl l) {
				(k, v) := str->splitstrr(hd l, "=");
				if(ishex(v)) {
					(name, ok) := readfile(sprint("/mnt/wmii/client/%s/props", v));
					if(ok)
						print("%d	%s%s\n", evn, k, name);
				}
			}
		}else
			print("%s", lin);
	}
}

