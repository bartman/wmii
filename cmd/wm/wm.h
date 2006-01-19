/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xutil.h>

#include "wmii.h"

/* array indexes of page file pointers */
enum {
    P_PREFIX,
    P_NAME,
    P_LAYOUT_PREFIX,
    P_SEL_LAYOUT,
    P_LAYOUT_NAME,
    P_CTL,
    P_LAST
};

/* array indexes of layout file pointers */
enum {
    L_PREFIX,
    L_FRAME_PREFIX,
    L_SEL_FRAME,
    L_NAME,
    L_CTL,
    L_LAST
};

/* array indexes of frame file pointers */
enum {
    F_PREFIX,
    F_NAME,
    F_GEOMETRY,
    F_BORDER,
    F_TAB,
    F_HANDLE_INC,
    F_LAST
};

/* array indexes of wm file pointers */
enum {
    WM_CTL,
    WM_TRANS_COLOR,
    WM_MANAGED_GEOMETRY,
    WM_SEL_BG_COLOR,
    WM_SEL_BORDER_COLOR,
    WM_SEL_FG_COLOR,
    WM_NORM_BG_COLOR,
    WM_NORM_BORDER_COLOR,
    WM_NORM_FG_COLOR,
    WM_FONT,
    WM_BORDER,
    WM_TAB,
    WM_HANDLE_INC,
    WM_SNAP_VALUE,
    WM_SEL_PAGE,
    WM_LAYOUT,
    WM_EVENT_PAGE_UPDATE,
    WM_EVENT_CLIENT_UPDATE,
    WM_EVENT_B1PRESS,
    WM_EVENT_B2PRESS,
    WM_EVENT_B3PRESS,
    WM_EVENT_B4PRESS,
    WM_EVENT_B5PRESS,
    WM_LAST
};

/* array indexes of EWMH window properties */
	                  /* TODO: set / react */
enum {
	NET_NUMBER_OF_DESKTOPS, /*  ✓      –  */
	NET_CURRENT_DESKTOP,    /*  ✓      ✓  */
	NET_WM_DESKTOP          /*  ✗      ✗  */
};

#define NET_ATOM_COUNT         3

#define PROTO_DEL              1
#define BORDER_WIDTH           3
#define LAYOUT                 "column"
#define LAYOUT_FLOAT           "float"
#define GAP                    5

#define ROOT_MASK              SubstructureRedirectMask
#define CLIENT_MASK            (StructureNotifyMask | PropertyChangeMask)

typedef struct Page Page;
typedef struct AttachQueue AttachQueue;
typedef struct LayoutDef LayoutDef;
typedef struct Layout Layout;
typedef struct Frame Frame;
typedef struct Client Client;

struct AttachQueue {
	Page *page;
	AttachQueue *next;
};

struct Page {
    Layout *managed;
    Layout *floating;
    Layout *sel;
    File *file[P_LAST];
    Page *next;
    Page *prev;
    size_t index;
};

struct LayoutDef {
    char *name;
    void (*init) (Layout *, Client *); /* called when layout is initialized */
    Client *(*deinit) (Layout *); /* called when layout is uninitialized */
    void (*arrange) (Layout *); /* called when layout is resized */
    Bool(*attach) (Layout *, Client *);  /* called on attach */
    void (*detach) (Layout *, Client *, Bool unmap); /* called on detach */
    void (*resize) (Frame *, XRectangle *, XPoint *); /* called after resize */
    void (*focus) (Layout *, Client *, Bool raise); /* focussing a client */
    Frame *(*frames) (Layout *); /* called for drawing */
    Client *(*sel) (Layout *); /* returns selected client */
    Action *(*actions) (Layout *); /* local action table */
    LayoutDef *next;
};

struct Layout {
    Page *page;
    LayoutDef *def;
    void *aux;                  /* auxillary pointer */
    File *file[L_LAST];
};

struct Frame {
    Layout *layout;
    Window win;
    Client *sel;
    Client *clients;
    size_t nclients;
    GC gc;
    XRectangle rect;
    Cursor cursor;
    void *aux;                  /* auxillary pointer */
    File *file[F_LAST];
    Frame *next;
    Frame *prev;
};

struct Client {
    int proto;
    unsigned int border;
    unsigned int ignore_unmap;
	char name[256];
    Bool destroyed;
    Window win;
    Window trans;
    XRectangle rect;
    XSizeHints size;
    Frame *frame;
    Client *next;
    Client *prev;
};


/* global variables */
Page *pages;
Page *selpage;
AttachQueue *attachqueue;
size_t npages;
int pageid;
Client *detached;
size_t ndetached;
LayoutDef *layouts;

Display *dpy;
IXPServer *ixps;
int screen_num;
Window root;
Window transient;
XRectangle rect;
XRectangle layout_rect;
XFontStruct *font;
XColor xorcolor;
GC xorgc;
GC transient_gc;

Atom wm_state; /* TODO: Maybe replace with wm_atoms[WM_ATOM_COUNT]? */
Atom wm_change_state;
Atom wm_protocols;
Atom wm_delete;
Atom motif_wm_hints;
Atom net_atoms[NET_ATOM_COUNT];

Cursor normal_cursor;
Cursor resize_cursor;
Cursor move_cursor;
Cursor drag_cursor;
Cursor w_cursor;
Cursor e_cursor;
Cursor n_cursor;
Cursor s_cursor;
Cursor nw_cursor;
Cursor ne_cursor;
Cursor sw_cursor;
Cursor se_cursor;

/* default file pointers */
File *def[WM_LAST];

unsigned int valid_mask, num_lock_mask;


/* client.c */
Client *alloc_client(Window w);
void init_client(Client * c, XWindowAttributes * wa);
void destroy_client(Client * c);
void configure_client(Client * c);
void handle_client_property(Client * c, XPropertyEvent * e);
void close_client(Client * c);
void draw_client(Client * client);
void draw_clients(Frame * f);
void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert);
void grab_client(Client * c, unsigned long mod, unsigned int button);
void ungrab_client(Client * c, unsigned long mod, unsigned int button);
void unmap_client(Client * c);
void map_client(Client * c);
void reparent_client(Client * c, Window w, int x, int y);
void attach_client(Client * c);
void detach_client(Client * c, Bool unmap);
Client *sel_client();
Client *clientat(Client * clients, size_t idx);
void detach_detached(Client * c);
void attach_detached(Client * c);
void focus_client(Client *new, Client *old);

/* frame.c */
Frame *win_to_frame(Window w);
Frame *alloc_frame(XRectangle * r);
void destroy_frame(Frame * f);
void resize_frame(Frame * f, XRectangle * r, XPoint * pt);
void draw_frame(Frame * f);
void handle_frame_buttonpress(XButtonEvent * e, Frame * f);
void attach_client_to_frame(Frame * f, Client * client);
void detach_client_from_frame(Client * client, Bool unmap);
unsigned int tab_height(Frame * f);
unsigned int border_width(Frame * f);
Frame *sel_frame();

/* event.c */
void init_event_hander();
void check_event(Connection * c);

/* mouse.c */
void mouse_resize(Frame * f, Align align);
void mouse_move(Frame * f);
Cursor cursor_for_motion(Frame * f, int x, int y);
Align cursor_to_align(Cursor cursor);
Align xy_to_align(XRectangle * rect, int x, int y);
void drop_move(Frame * f, XRectangle * new, XPoint * pt);

/* page.c */
Page *pageat(unsigned int idx);
Page *alloc_page();
void destroy_page(Page * p);
void focus_page(Page * p);
XRectangle *rectangles(unsigned int *num);

/* layout.c */
Layout *alloc_layout(Page * p, char *layout);
void destroy_layout(Layout *l);
void focus_layout(Layout *l);
void unmap_layout(Layout *l);
void map_layout(Layout *l, Bool raise);
Layout *sel_layout();
void attach_frame_to_layout(Layout *l, Frame * f);
void detach_frame_from_layout(Frame * f);
LayoutDef *match_layout_def(char *name);

/* layoutdef.c */
void init_layouts();

/* wm.c */
void invoke_wm_event(File * f);
void run_action(File * f, void *obj, Action * acttbl);
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
void handle_after_write(IXPServer * s, File * f);
void detach(Frame * f, int client_destroyed);
void set_client_state(Client * c, int state);
