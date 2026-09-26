/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
/* Mirrors dwmr/src/dwm.rs, with the types of dwmr/src/config.rs */
/* Mirrors dwmr/src/main.rs: main() at the end */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/extensions/XRes.h>
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define GETINC(X)               ((X) - 2000)
#define INC(X)                  ((X) + 2000)
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISINC(X)                ((X) > 1000 && (X) < 3000)
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define MOD(N,M)                ((N)%(M) < 0 ? (N)%(M) + (M) : (N)%(M))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define NUMTAGS                 (LENGTH(tags) + LENGTH(scratchpads))
#define TAGMASK                 ((1U << NUMTAGS) - 1)
#define SPTAG(i)                ((1U << LENGTH(tags)) << (i))
#define SPTAGMASK               (((1U << LENGTH(scratchpads)) - 1) << LENGTH(tags))
#define TAGBITS                 ((1U << LENGTH(tags)) - 1) /* normal tags, no scratchpads */
#define SCREEN_MASK             (0x55555555U & TAGBITS)    /* odd tags 1,3,5,..: first monitor */
#define TEXTW(X)                ((int)drw_fontset_getwidth(drw, (X)) + lrpad)
#define STATUSCLRS              64 /* ^c#rrggbb^ status colors kept allocated before starting over */

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetWMSticky, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkClientWin,
       ClkRootWin, ClkLast }; /* clicks */
enum { Col1, Col21, Col22, Col23, Col24, Col3, Col4, Col5, Col6,
       ColLast }; /* status text colors (status2d) */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float cfact; /* weight in its area of the layout (cfacts), see setcfact() */
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky, isterminal, noswallow, isbrowser;
	pid_t pid;
	Client *next;
	Client *snext;
	Client *swallowing; /* the client this terminal swallowed; not in any client or stack list */
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappih;           /* horizontal gap between windows */
	int gappiv;           /* vertical gap between windows */
	int gappoh;           /* horizontal outer gaps */
	int gappov;           /* vertical outer gaps */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isterminal;
	int noswallow;
	int monitor;
} Rule;

/* A scratchpad: a named command whose window is tagged with its own tag
 * bit, SPTAG(i), above the normal tags. */
typedef struct {
	const char *name;
	const void *cmd;
} Sp;

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	const char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayout(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatusbar(Monitor *m, int bh, const char *text);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusnthmon(const Arg *arg);
static void focusurgent(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static pid_t getstatusbarpid(void);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void layoutmenu(const Arg *arg);
static void load_xresources(void);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void notifysend(const char *tag, const char *msg);
static Monitor *numtomon(int num);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void pushstack(const Arg *arg);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resource_load(XrmDatabase db, const char *name, enum resource_type rtype, void *dst);
static void restack(Monitor *m);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void sendmonview(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setsticky(Client *c, int sticky);
static void setcfact(const Arg *arg);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static unsigned int shifttags(unsigned int t, int i);
static void shifttag(const Arg *arg);
static void shiftview(const Arg *arg);
static void shiftviewclients(const Arg *arg);
static void showhide(Client *c);
static void sigstatusbar(const Arg *arg);
static void spawn(const Arg *arg);
static int stackpos(const Arg *arg);
static Clr *statuscolor(const char *hex);
static void statusfontcode(const char *code);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tagmonview(const Arg *arg);
static void tagnewmon(const Arg *arg);
static void tagnextmon(const Arg *arg);
static void tagnthmonview(const Arg *arg);
static void tagview(const Arg *arg);
static void togglebar(const Arg *arg);
static void togglebars(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglelayoutalltags(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglesticky(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenetwmstate(Client *c);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Clr *weathercolor(const char *s);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* swallow */
static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static int isstatusbar(pid_t pid);
static void swallow(Client *p, Client *c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static void unswallow(Client *c);
static pid_t winpid(Window w);

/* vanitygaps.c: gaps and the gap-aware layouts */
static void bstack(Monitor *m);
static void centeredfloatingmaster(Monitor *m);
static void centeredmaster(Monitor *m);
static void deck(Monitor *m);
static void defaultgaps(const Arg *arg);
static void dwindle(Monitor *m);
static void fibonacci(Monitor *m, int s);
static int ftoi(float f);
static void getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr, int *sr);
static void getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, int *nc);
static void incrgaps(const Arg *arg);
static void setgaps(int oh, int ov, int ih, int iv);
static void spiral(Monitor *m);
static void tile(Monitor *m);
static void togglebgaps(const Arg *arg);
static void togglegaps(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static char stext[1024];
static int statusw;          /* width of the status text, set by drawbar() */
static int statussig;        /* signal byte of the clicked status block, 0 for none */
static pid_t statuspid = -1; /* the status bar's pid, from getstatusbarpid() */
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
/* the colors of the status text codes, allocated once in setup(); dwm's
 * status2d allocates one at every code on every redraw */
static Clr *statusclr;
static struct {
	unsigned int rgb;
	Clr clr;
} statusclrs[STATUSCLRS]; /* the ^c#rrggbb^ colors seen so far */
static int nstatusclrs;
static Display *dpy;
static Drw *drw;
static Fnt *normalfont, *statusbigfont; /* ^N^ and ^B^ in the status text */
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time checks of config.h, for what dwmr's config loader rejects */
/* all tags and scratchpad tags fit into an unsigned int bit array ("tags and
 * scratchpads together must not exceed 31"), and there is a tag */
struct NumTags { char limitexceeded[LENGTH(tags) < 1 || NUMTAGS > 31 ? -1 : 1]; };
/* "fonts must not be empty" */
struct NumFonts { char nofonts[LENGTH(fonts) < 1 ? -1 : 1]; };
/* "layouts must not be empty" */
struct NumLayouts { char nolayouts[LENGTH(layouts) < 1 ? -1 : 1]; };

/* the index in layouts[] of each tag's layout, used when !layoutalltags */
static unsigned int taglayouts[LENGTH(tags)];
/* "colors must define the SchemeNorm and SchemeSel schemes" */
struct NumColors { char noschemes[LENGTH(colors) < 2 ? -1 : 1]; };
/* The scalars dwmr checks (gaps at most GAP_MAX, refreshrate at least 1) are
 * variables here, not constant expressions, so they are checked where they
 * are used: createmon() and setgaps() clamp the gaps, movemouse() and
 * resizemouse() the refresh rate; togglescratch() checks its index. */

/* vanitygaps.c, which the patch #includes from config.h */
#include "vanitygaps.c"

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;
	/* firefox, Firefox, firefox-esr, ... (used by getgaps) */
	c->isbrowser = !strncasecmp(class, "firefox", 7);

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isterminal = r->isterminal;
			c->noswallow  = r->noswallow;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			if ((r->tags & SPTAGMASK) && r->isfloating) {
				c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
				c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
			}

			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : (c->mon->tagset[c->mon->seltags] & ~SPTAGMASK);
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5f;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5f;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value; x % -1 is 0, and INT_MIN % -1 undefined */
		if (c->incw && c->incw != -1)
			*w -= *w % c->incw;
		if (c->inch && c->inch != -1)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	unsigned int i;

	/* a layout per tag: the one of the first viewed tag */
	for (i = 0; !layoutalltags && running && i < LENGTH(tags); i++)
		if (m->tagset[m->seltags] & 1 << i) {
			m->lt[m->sellt] = &layouts[taglayouts[i]];
			break;
		}
	truncate_utf8(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
swallow(Client *p, Client *c)
{
	Window w;
	int b;

	if (c->noswallow || c->isterminal)
		return;
	if (!swallowfloating && c->isfloating)
		return;

	detach(c);
	detachstack(c);

	setclientstate(c, WithdrawnState);
	XUnmapWindow(dpy, p->win);

	p->swallowing = c;
	c->mon = p->mon;

	w = p->win;
	p->win = c->win;
	c->win = w;
	b = p->isbrowser;
	p->isbrowser = c->isbrowser;
	c->isbrowser = b;
	updatetitle(p);
	XMoveResizeWindow(dpy, p->win, p->x, p->y, MAX(p->w, 1), MAX(p->h, 1));
	arrange(p->mon);
	configure(p);
	updateclientlist();
}

void
unswallow(Client *c)
{
	if (!c->swallowing)
		return;
	c->win = c->swallowing->win;
	c->isbrowser = c->swallowing->isbrowser;

	free(c->swallowing);
	c->swallowing = NULL;

	/* unfullscreen the client */
	setfullscreen(c, 0);
	updatetitle(c);
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	XMoveResizeWindow(dpy, c->win, c->x, c->y, MAX(c->w, 1), MAX(c->h, 1));
	setclientstate(c, NormalState);
	focus(NULL);
	arrange(c->mon);
}

void
buttonpress(XEvent *e)
{
	unsigned int i, click, occ = 0;
	int x;
	char *text, *s, ch;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon
	&& (focusonwheel || (ev->button != Button4 && ev->button != Button5))) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		for (c = selmon->clients; c; c = c->next)
			occ |= c->tags == TAGBITS ? 0 : c->tags;
		do {
			/* Do not reserve space for vacant tags */
			if (!(occ & 1 << i || selmon->tagset[selmon->seltags] & 1 << i))
				continue;
			x += TEXTW(tags[i]);
		} while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + TEXTW(selmon->ltsymbol))
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - statusw) {
			x = selmon->ww - statusw;
			click = ClkStatusText;

			statussig = 0;
			for (text = s = stext; *s && x <= ev->x; s++) {
				if ((unsigned char)*s < ' ') {
					ch = *s;
					*s = '\0';
					x += TEXTW(text) - lrpad;
					*s = ch;
					text = s + 1;
					if (x >= ev->x)
						break;
					statussig = ch;
				} else if (*s == '^') {
					*s = '\0';
					x += TEXTW(text) - lrpad;
					*s = '^';
					/* measure with the font the text was drawn with (^B^/^N^) */
					statusfontcode(s + 1);
					/* no ^f^ (forward) here: drawstatusbar() ignores it too */
					for (s++; *s && *s != '^'; s++);
					if (!*s)
						break; /* unterminated ^ code */
					text = s + 1;
				}
			}
			drw_setfontset(drw, normalfont);
		}
		/* notitle: the space between the layout symbol and the status is
		 * no click target; click stays ClkRootWin */
	} else if ((c = wintoclient(ev->window))) {
		if (focusonwheel || (ev->button != Button4 && ev->button != Button5))
			/* deliberately no restack() here, unlike dwm and the focusonclick
			 * patch: a click focuses a floating window without raising it */
			focus(c);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		drw_scm_free(drw, scheme[i], 3);
	free(scheme);
	drw_scm_free(drw, statusclr, ColLast);
	for (i = 0; i < (size_t)nstatusclrs; i++)
		drw_clr_free(drw, &statusclrs[i].clr);
	nstatusclrs = 0;
	XDestroyWindow(dpy, wmcheckwin);
	/* drw_free() frees the current font set, so make it the normal one */
	drw_setfontset(drw, normalfont);
	drw_fontset_free(statusbigfont);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		if (m)
			m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if ((Atom)cme->data.l[1] == netatom[NetWMFullscreen]
		|| (Atom)cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));

		if ((Atom)cme->data.l[1] == netatom[NetWMSticky]
		|| (Atom)cme->data.l[2] == netatom[NetWMSticky])
			setsticky(c, (cme->data.l[0] == 1 || (cme->data.l[0] == 2 && !c->issticky)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.serial = 0;
	ce.send_event = False;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, MAX(sw, 1), MAX(bh, 1));
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	/* odd tags on the first monitor, even tags on the second */
	if (mons)
		m->tagset[0] = m->tagset[1] = 2;
	else
		m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	/* X resources may set any gap: keep them within what setgaps() allows */
	m->gappih = MIN(gappih, GAP_MAX);
	m->gappiv = MIN(gappiv, GAP_MAX);
	m->gappoh = MIN(gappoh, GAP_MAX);
	m->gappov = MIN(gappov, GAP_MAX);
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	truncate_utf8(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = swallowingclient(ev->window)) && c->swallowing)
		unmanage(c->swallowing, 1);
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	if (*tc) /* not in the list: leave the list alone, as dwmr does */
		*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	if (*tc)
		*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	int x, w, tw = 0;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	if (!m->showbar)
		return;

	/* draw status first so it can be overdrawn by tags later */
	/* status is drawn on every monitor, not just selmon */
	tw = statusw = m->ww - drawstatusbar(m, bh, stext);

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags == TAGBITS ? 0 : c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		/* Do not draw vacant tags */
		if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
			continue;
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], !!(urg & 1 << i));
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > bh) {
		/* notitle: no window title, just clear the rest of the bar */
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w, bh, 1, 1);
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

/* Draw the status text right-aligned on monitor m (status2d). "^..^" codes
 * are not drawn but switch the text color (col1..col6, ^c#rrggbb^) or the
 * font (see statusfontcode()). Returns the x where the status starts. */
int
drawstatusbar(Monitor *m, int bh, const char *text)
{
	int ret, w, x, iscode = 0;
	static char buf[sizeof stext]; /* stext without the signal bytes */
	char hex[8], *p, *s;
	const char *t;
	Clr sc[3], *clr;

	/* strip the signal bytes that mark clickable status blocks */
	for (p = buf, t = text; *t && p < buf + sizeof buf - 1; t++)
		if ((unsigned char)*t >= ' ')
			*p++ = *t;
	*p = '\0';

	/* compute width of the status text */
	w = 0;
	for (s = p = buf; *p; p++) {
		if (*p == '^') {
			if (!iscode) {
				iscode = 1;
				*p = '\0';
				w += TEXTW(s) - lrpad;
				*p = '^';
				statusfontcode(p + 1);
			} else {
				iscode = 0;
				s = p + 1;
			}
		}
	}
	if (!iscode)
		w += TEXTW(s) - lrpad;
	drw_setfontset(drw, normalfont);

	w += 2; /* 1px padding on both sides */
	ret = x = m->ww - w;

	/* the status is drawn with a copy of SchemeNorm; the codes change its
	 * fg, not the scheme's */
	memcpy(sc, scheme[SchemeNorm], sizeof sc);
	drw_setscheme(drw, sc);
	drw_rect(drw, x, 0, w, bh, 1, 1);
	x++;

	/* process status text */
	sc[ColFg] = statusclr[Col1];
	for (s = p = buf; *p; p++) {
		if (*p == '^') {
			iscode = 1;

			*p = '\0';
			w = TEXTW(s) - lrpad;
			drw_text(drw, x, 0, w, bh, 0, s, 0);
			*p = '^';
			x += w;
			statusfontcode(p + 1);

			/* process code */
			for (p++; *p && *p != '^'; p++) {
				switch (*p) {
				case '2':
					/* weather: color by the temperature that follows */
					t = strchr(p, '^');
					sc[ColFg] = *weathercolor(t ? t + 1 : "");
					break;
				case '3': sc[ColFg] = statusclr[Col3]; break;
				case '4': sc[ColFg] = statusclr[Col4]; break;
				case '5': sc[ColFg] = statusclr[Col5]; break;
				case '6': sc[ColFg] = statusclr[Col6]; break;
				case 'c':
					if (p[1] == '#' && strspn(p + 2, "0123456789abcdefABCDEF") >= 6) {
						/* ^c#rrggbb^: any foreground color */
						memcpy(hex, p + 1, 7);
						hex[7] = '\0';
						if ((clr = statuscolor(hex)))
							sc[ColFg] = *clr;
						p += 7;
					}
					break;
				default: /* ^r, ^b, ^d, ^f and unknown codes are ignored */
					break;
				}
			}
			if (!*p)
				break; /* unterminated ^ code */

			s = p + 1;
			iscode = 0;
		}
	}

	if (!iscode) {
		w = TEXTW(s) - lrpad;
		drw_text(drw, x, 0, w, bh, 0, s, 0);
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_setfontset(drw, normalfont);

	return ret;
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusnthmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = numtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, m->wx + m->ww / 2, m->wy + m->wh / 2);
	selmon = m;
	focus(NULL);
}

/* Jump to the first urgent client on any monitor: view its tag on its own
 * monitor and focus it. The tagset is set here instead of calling view(),
 * whose odd/even split would pick the monitor from the tag, not the client. */
void
focusurgent(const Arg *arg)
{
	Monitor *m;
	Client *c = NULL;
	unsigned int i;

	for (m = mons; m && !c; m = m->next)
		for (c = m->clients; c && !(c->isurgent && (c->tags & TAGBITS)); c = c->next);
	if (!c) {
		notifysend("urgent", "no urgent window");
		return;
	}
	if (c->mon != selmon) {
		unfocus(selmon->sel, 0);
		selmon = c->mon;
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->wx + selmon->ww / 2, selmon->wy + selmon->wh / 2);
	}
	if (!ISVISIBLE(c)) {
		for (i = 0; !(c->tags & 1 << i); i++);
		selmon->seltags ^= 1;
		selmon->tagset[selmon->seltags] = 1 << i;
		arrange(selmon);
	}
	focus(c);
	restack(selmon); /* also drops the EnterNotify from the pointer warp */
}

void
focusstack(const Arg *arg)
{
	int i = stackpos(arg);
	Client *c, *p;

	if (i < 0 || (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
		return;

	for (p = NULL, c = selmon->clients; c && (i || !ISVISIBLE(c));
	    i -= ISVISIBLE(c) ? 1 : 0, p = c, c = c->next);
	focus(c ? c : p);
	restack(selmon);
}

Atom
getatomprop(Client *c, Atom prop)
{
	int format;
	unsigned long nitems, dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &format, &nitems, &dl, &p) == Success && p) {
		if (nitems > 0 && format == 32)
			atom = *(long *)p;
		XFree(p);
	}
	return atom;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, &p) != Success)
		return -1;
	if (n != 0 && format == 32 && p)
		result = *(long *)p;
	if (p)
		XFree(p);
	return result;
}

/* whether argv[0] of process pid, without its directory, is statusbar */
static int
isstatusbar(pid_t pid)
{
	char path[32], buf[4096], *base, *s;
	ssize_t n;
	int fd;

	snprintf(path, sizeof path, "/proc/%d/cmdline", (int)pid);
	if ((fd = open(path, O_RDONLY)) < 0)
		return 0;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	for (base = s = buf; *s; s++)
		if (*s == '/')
			base = s + 1;
	return *base && !strcmp(base, statusbar);
}

/* The pid of the status bar program (statusbar), -1 if it is not running.
 * The last one found is reused while its argv[0] still names the status
 * bar; otherwise /proc is searched like pidof -s does. */
pid_t
getstatusbarpid(void)
{
	DIR *dir;
	struct dirent *ent;
	char *end;
	long pid = -1;

	if (statuspid > 0 && isstatusbar(statuspid))
		return statuspid;
	if (!(dir = opendir("/proc")))
		return -1;
	while ((ent = readdir(dir))) {
		pid = strtol(ent->d_name, &end, 10);
		if (end != ent->d_name && !*end && pid > 0 && pid <= INT_MAX && isstatusbar(pid))
			break;
	}
	closedir(dir);
	return ent ? (pid_t)pid : -1;
}

/* Read a text property into text, at most size - 1 bytes, cut at a
 * character boundary. */
int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom))
		return 0;
	if (!name.nitems || !name.value) {
		/* dwm and dwmr return without freeing an empty property's value */
		if (name.value)
			XFree(name.value);
		return 0;
	}
	if (name.encoding == XA_STRING) {
		truncate_utf8(text, (char *)name.value, size);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && list) {
		if (n > 0 && *list)
			truncate_utf8(text, *list, size);
		XFreeStringList(list);
	}
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		int k, start, end, skip;
		KeySym *syms;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		XDisplayKeycodes(dpy, &start, &end);
		syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey(dpy, k,
							 keys[i].mod | modifiers[j],
							 root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}

void
incnmaster(const Arg *arg)
{
	char msg[32];

	/* nmaster may be any int X resources set: saturate instead of overflowing */
	if (arg->i > 0 && selmon->nmaster > INT_MAX - arg->i)
		selmon->nmaster = INT_MAX;
	else if (arg->i < 0 && selmon->nmaster < INT_MIN - arg->i)
		selmon->nmaster = 0;
	else
		selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
	snprintf(msg, sizeof msg, "master: %d", selmon->nmaster);
	notifysend("nmaster", msg);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

/* Apply the X resources dwm.<name> (xresources patch) on top of config.h.
 * Called before setup(). */
void
load_xresources(void)
{
	char *resm;
	XrmDatabase db;
	const ResourcePref *p;

	if (!(resm = XResourceManagerString(dpy)))
		return;
	if (!(db = XrmGetStringDatabase(resm)))
		return;
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XrmDestroyDatabase(db);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL, *term = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;
	c->mon = selmon;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
		term = termforwin(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (term)
		swallow(term, c);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%u]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (Time)(1000 / MAX(refreshrate, 1)))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < (int)snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < (int)snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < (int)snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < (int)snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > (int)snap || abs(ny - c->y) > (int)snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

/* a notification (dunst); a new one replaces the last one with the same tag
 * instead of stacking */
void
notifysend(const char *tag, const char *msg)
{
	char hint[64];

	snprintf(hint, sizeof hint, "string:x-dunst-stack-tag:%s", tag);
	spawn(&((Arg) { .v = (const char *[]){ "notify-send", "-t", "2000", "-h", hint,
		"dwmc", msg, NULL } }));
}

/* The monitor at position num in the monitor list, or the last one if
 * there are fewer. */
Monitor *
numtomon(int num)
{
	Monitor *m;
	int i;

	for (m = mons, i = 0; m->next && i < num; m = m->next)
		i++;
	return m;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName])
			updatetitle(c); /* notitle: the bar does not show it */
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
pushstack(const Arg *arg)
{
	int i = stackpos(arg);
	Client *sel = selmon->sel, *c, *p;

	if (i < 0 || !sel)
		return;
	else if (i == 0) {
		detach(sel);
		attach(sel);
	} else {
		for (p = NULL, c = selmon->clients; c; p = c, c = c->next)
			if (!(i -= (ISVISIBLE(c) && c != sel)))
				break;
		/* c is set here: the list is not empty, so p is the last client if
		 * the walk ran out; pushing sel after itself is a no-op */
		if ((c = c ? c : p)) {
			detach(sel);
			sel->next = c->next;
			c->next = sel;
		}
	}
	arrange(selmon);
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	/* noborder: the only visible tiled client, or any client in monocle,
	 * fills the space the border would take; c->bw itself is kept */
	if (((nexttiled(c->mon->clients) == c && !nexttiled(c->next))
	    || &monocle == c->mon->lt[c->mon->sellt]->arrange)
	    && !c->isfullscreen && !c->isfloating) {
		c->w = wc.width += c->bw * 2;
		c->h = wc.height += c->bw * 2;
		wc.border_width = 0;
	}
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (Time)(1000 / MAX(refreshrate, 1)))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > (int)snap || abs(nh - c->h) > (int)snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

/* Look up dwm.<name> (class *) in db and store it in dst. A value that
 * does not parse leaves dst as it is, where the patch would store 0. */
void
resource_load(XrmDatabase db, const char *name, enum resource_type rtype, void *dst)
{
	char fullname[256], *type, *s, *end;
	XrmValue ret;
	unsigned long u;

	snprintf(fullname, sizeof fullname, "dwm.%s", name);
	if (!XrmGetResource(db, fullname, "*", &type, &ret)
	|| !ret.addr || !type || strcmp(type, "String"))
		return;
	switch (rtype) {
	case STRING:
		/* all STRING resources are "#RRGGBB" char[8] buffers */
		if (strlen(ret.addr) < 8)
			memcpy(dst, ret.addr, strlen(ret.addr) + 1);
		break;
	case INTEGER:
		u = strtoul(ret.addr, &end, 10);
		if (end != ret.addr)
			*(unsigned int *)dst = u;
		break;
	case FLOAT:
		/* a decimal number only, like dwmr: no hexadecimal, inf or nan */
		for (s = ret.addr; *s == ' ' || (*s >= '\t' && *s <= '\r'); s++);
		if (*s == '+' || *s == '-')
			s++;
		if (!(*s >= '0' && *s <= '9') && !(*s == '.' && s[1] >= '0' && s[1] <= '9'))
			break;
		*(float *)dst = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 0 : strtof(ret.addr, NULL);
		break;
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (ev.type >= 0 && ev.type < LASTEvent && handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

/* Run the autostart command of config.h once at startup (my dwm's
 * runautostart() calls system("killall -q dwmblocks; dwmblocks &")). It
 * goes through spawn(), so it does not block and the child is reaped like
 * every other program dwmc starts. { NULL } runs nothing. */
void
runautostart(void)
{
	Arg a = {.v = autostart};

	spawn(&a);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; wins && i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; wins && i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	/* assign tags of target monitor, without any visible scratchpad tags */
	if (!(c->tags = m->tagset[m->seltags] & ~SPTAGMASK))
		c->tags = 1;
	attach(c);
	attachstack(c);
	if (c->isfullscreen)
		resizeclient(c, m->mx, m->my, m->mw, m->mh);
	focus(NULL);
	arrange(NULL);
}

void
sendmonview(Client *c, Monitor *m)
{
	unsigned int allowed, t;

	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	arrange(c->mon);
	c->mon = m;
	/* assign tags of target monitor, without any visible scratchpad tags;
	 * with more than one monitor only the tags of m's parity (odd tags on
	 * the first monitor, even tags on the others), so a target viewing
	 * all tags does not put c on a tag of the wrong monitor */
	if (!mons->next)
		allowed = ~0U;
	else if (m == mons)
		allowed = SCREEN_MASK;
	else
		allowed = ~SCREEN_MASK & TAGBITS;
	t = m->tagset[m->seltags] & ~SPTAGMASK & allowed;
	if (t)
		c->tags = t;
	else if (allowed)
		c->tags = allowed & -allowed; /* the lowest allowed tag */
	else
		c->tags = 1;
	attach(c);
	attachstack(c);
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, m->wx + m->ww / 2, m->wy + m->wh / 2);
	arrange(m);
	focus(c);
	restack(m);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		memset(&ev, 0, sizeof ev);
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus)
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XChangeProperty(dpy, root, netatom[NetActiveWindow],
		XA_WINDOW, 32, PropModeReplace,
		(unsigned char *) &(c->win), 1);
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		updatenetwmstate(c);
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		c->isfullscreen = 0;
		updatenetwmstate(c);
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setsticky(Client *c, int sticky)
{
	if (sticky && !c->issticky) {
		c->issticky = 1;
		updatenetwmstate(c);
	} else if (!sticky && c->issticky) {
		c->issticky = 0;
		updatenetwmstate(c);
		arrange(c->mon);
	}
}

/* Set the layout arg->i places after the current one in layouts[],
 * wrapping around (dwm's cyclelayouts patch). */
void
cyclelayout(const Arg *arg)
{
	int n = LENGTH(layouts), i;

	for (i = 0; i < n && &layouts[i] != selmon->lt[selmon->sellt]; i++);
	if (i == n) /* not one of layouts[]: from the first */
		i = 0;
	setlayout(&((Arg) { .v = &layouts[((i + arg->i) % n + n) % n] }));
}

/* Run the command arg->v, which prints the index in layouts[] of the
 * layout to set, with the current one's index in LAYOUT_MENU_CURRENT
 * (dwm's layoutmenu patch). dwmc waits for it to exit, like for a menu. */
void
layoutmenu(const Arg *arg)
{
	char *const *argv = (char *const *)arg->v;
	char out[32], buf[256], cur[16], *end = out;
	struct sigaction sa;
	int fd[2], n = LENGTH(layouts), i;
	size_t len = 0;
	ssize_t r;
	pid_t pid;

	if (!argv || !argv[0] || pipe(fd) < 0)
		return;
	for (i = 0; i < n && &layouts[i] != selmon->lt[selmon->sellt]; i++);
	snprintf(cur, sizeof cur, "%d", i == n ? 0 : i);
	if ((pid = fork()) == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		close(fd[0]);
		if (fd[1] != STDOUT_FILENO) {
			dup2(fd[1], STDOUT_FILENO);
			close(fd[1]);
		}
		/* the command must not inherit dwmc's ignored SIGCHLD */
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);
		setenv("LAYOUT_MENU_CURRENT", cur, 1);
		execvp(argv[0], argv);
		die("dwmc: execvp '%s' failed:", argv[0]);
	}
	close(fd[1]);
	/* read all of it, keep the start */
	while (pid > 0 && (r = read(fd[0], buf, sizeof buf)) != 0) {
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if ((size_t)r > sizeof out - 1 - len)
			r = sizeof out - 1 - len;
		memcpy(out + len, buf, r);
		len += r;
	}
	close(fd[0]);
	/* SIGCHLD is ignored: this waits for the exit, then fails with ECHILD */
	if (pid > 0)
		waitpid(pid, NULL, 0);
	out[len] = '\0';
	i = strtol(out, &end, 10);
	if (end != out && i >= 0 && i < n)
		setlayout(&((Arg) { .v = &layouts[i] }));
}

/* cfacts: change the focused client's weight in its area of the layout (its
 * height in a column of tile, deck and centeredmaster, its width in bstack
 * and centeredfloatingmaster) by arg->f, within 0.25..4.0; 0 sets it back
 * to 1.0 */
void
setcfact(const Arg *arg)
{
	float f;
	Client *c = selmon->sel;

	if (!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if (arg->f == 0.0)
		f = 1.0;
	else if (f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

void
setlayout(const Arg *arg)
{
	const Layout *l = NULL;
	unsigned int i;
	Monitor *m;

	/* only a layout of layouts[]: dwmr rejects any other index */
	for (i = 0; arg && arg->v && i < LENGTH(layouts); i++)
		if (arg->v == &layouts[i])
			l = &layouts[i];
	if (!l || l != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (l)
		selmon->lt[selmon->sellt] = l;
	/* every tag and monitor takes it (layoutalltags), else the viewed tags */
	for (i = 0; i < LENGTH(tags); i++)
		if (layoutalltags || selmon->tagset[selmon->seltags] & 1 << i)
			taglayouts[i] = selmon->lt[selmon->sellt] - layouts;
	for (m = mons; layoutalltags && m; m = m->next)
		if (m != selmon) {
			m->lt[m->sellt] = selmon->lt[selmon->sellt];
			arrange(m);
		}
	truncate_utf8(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (!(f >= 0.05f && f <= 0.95f)) /* NaN too */
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
setup(void)
{
	size_t i;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;
	const char *statuscolors[ColLast] = {
		[Col1] = col1, [Col21] = col21, [Col22] = col22, [Col23] = col23,
		[Col24] = col24, [Col3] = col3, [Col4] = col4, [Col5] = col5, [Col6] = col6,
	};

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, MAX(sw, 1), MAX(sh, 1));
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	/* creating a font set makes it the current one, so switch back; the
	 * big status font is optional: without it ^B^ does nothing */
	normalfont = drw->fonts;
	statusbigfont = drw_fontset_create(drw, statusbigfonts, LENGTH(statusbigfonts));
	drw_setfontset(drw, normalfont);
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMSticky] = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	/* status2d: the colors of the status text codes, allocated once */
	statusclr = drw_scm_create(drw, statuscolors, ColLast);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwmc", 4);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

/* The circular shift of t by i within the normal tags (the shift-tools
 * patch, scratchpads variant): i > 0 is a left, i < 0 a right circular
 * shift, and the result is masked to the normal tag bits. dwm shifts by
 * arg->i as given; reducing it modulo LENGTH(tags) gives the same rotation
 * and keeps every shift count below 32 for any configured value. */
unsigned int
shifttags(unsigned int t, int i)
{
	unsigned int k = MOD(i, (int)LENGTH(tags));

	if (!k)
		return t & TAGBITS;
	return ((t << k) | (t >> (LENGTH(tags) - k))) & TAGBITS;
}

/* Sends a window to the next/prev tag */
void
shifttag(const Arg *arg)
{
	Arg shifted;

	shifted.ui = shifttags(selmon->tagset[selmon->seltags] & ~SPTAGMASK, arg->i);
	tag(&shifted);
}

/* Navigate to the next/prev tag */
void
shiftview(const Arg *arg)
{
	Arg shifted;

	shifted.ui = shifttags(selmon->tagset[selmon->seltags] & ~SPTAGMASK, arg->i);
	view(&shifted);
}

/* Navigate to the next/prev tag that has a client, else moves it to the next/prev tag */
void
shiftviewclients(const Arg *arg)
{
	Arg shifted;
	Client *c;
	unsigned int i, tagmask = 0;

	shifted.ui = selmon->tagset[selmon->seltags] & ~SPTAGMASK;
	for (c = selmon->clients; c; c = c->next)
		if (!(c->tags & SPTAGMASK))
			tagmask |= c->tags;

	/* dwm shifts until the result hits an occupied tag. The rotation
	 * repeats after LENGTH(tags) steps, so stop there instead of spinning
	 * when it never hits one: a view of only a scratchpad tag shifts to
	 * nothing, and a shift by a multiple of LENGTH(tags) stays put. */
	for (i = 0; i < LENGTH(tags); i++) {
		shifted.ui = shifttags(shifted.ui, arg->i);
		if (!tagmask || shifted.ui & tagmask)
			break;
	}
	view(&shifted);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		if ((c->tags & SPTAGMASK) && c->isfloating) {
			c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
			c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
		}
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

/* Send the clicked status block's signal (statussig) to the status bar,
 * with the button (arg->i) as the value, so it runs the block's command
 * with BLOCK_BUTTON set (statuscmd). */
void
sigstatusbar(const Arg *arg)
{
	union sigval sv;

	if (!statussig)
		return;
	sv.sival_int = arg->i;
	if ((statuspid = getstatusbarpid()) <= 0)
		return;

	sigqueue(statuspid, SIGRTMIN+statussig, sv);
}

/* appended to sh -c commands: the shell exits 127 when the command isn't found */
static const char notfoundcmd[] =
	"\n[ $? -ne 127 ] || notify-send -u critical \"dwmc: command not found\" \"$0\"";

void
spawn(const Arg *arg)
{
	struct sigaction sa;
	char *const *argv = (char *const *)arg->v;
	char *script, *shargv[] = { "/bin/sh", "-c", NULL, NULL, NULL };
	int err;

	/* an empty command runs nothing */
	if (!argv || !argv[0])
		return;
	if (arg->v == dmenucmd) /* the argument after "-m": the selected monitor */
		snprintf(dmenumon, sizeof dmenumon, "%d", selmon->num);
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		/* sh -c commands (SHCMD) tell when the command isn't found; the
		 * original command is $0, for the notification */
		if (!strcmp(argv[0], "/bin/sh") && argv[1] && !strcmp(argv[1], "-c")
		&& argv[2] && !argv[3]) {
			script = ecalloc(strlen(argv[2]) + sizeof notfoundcmd, 1);
			strcat(strcpy(script, argv[2]), notfoundcmd);
			shargv[2] = script;
			shargv[3] = argv[2];
			argv = shargv;
		}
		execvp(argv[0], argv);
		err = errno;
		execlp("notify-send", "notify-send", "-u", "critical", err == ENOENT
			? "dwmc: command not found" : "dwmc: can't run", argv[0], (char *)NULL);
		errno = err;
		die("dwmc: execvp '%s' failed:", argv[0]);
	}
}

int
stackpos(const Arg *arg)
{
	int n, i;
	Client *c;

	if (!selmon->clients)
		return -1;

	if (ISINC(arg->i)) {
		if (!selmon->sel)
			return -1;
		for (i = 0, c = selmon->clients; c != selmon->sel; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
		for (n = i; c; n += ISVISIBLE(c) ? 1 : 0, c = c->next);
		if (!n) /* sel is never invisible; guards the division anyway */
			return -1;
		return MOD(i + GETINC(arg->i), n);
	} else if (arg->i < 0) {
		for (i = 0, c = selmon->clients; c; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
		return MAX(i + arg->i, 0);
	} else
		return arg->i;
}

/* The color of a ^c#rrggbb^ status code (hex is the "#rrggbb"), allocated
 * the first time it is seen and kept for the next redraws; NULL when X cannot
 * allocate it. */
Clr *
statuscolor(const char *hex)
{
	unsigned int rgb;
	int i;

	rgb = strtoul(hex + 1, NULL, 16);
	for (i = 0; i < nstatusclrs; i++)
		if (statusclrs[i].rgb == rgb)
			return &statusclrs[i].clr;
	if (nstatusclrs >= STATUSCLRS) {
		/* a status that cycles through colors: start over */
		for (i = 0; i < nstatusclrs; i++)
			drw_clr_free(drw, &statusclrs[i].clr);
		nstatusclrs = 0;
	}
	if (!drw_clr_alloc(drw, &statusclrs[nstatusclrs].clr, hex))
		return NULL;
	statusclrs[nstatusclrs].rgb = rgb;
	return &statusclrs[nstatusclrs++].clr;
}

/* ^B^ switches the status text to the big font (e.g. for a block's icon),
 * ^N^ back to the normal one; code points just past the opening '^' */
void
statusfontcode(const char *code)
{
	if (code[0] == 'B' && code[1] == '^' && statusbigfont)
		drw_setfontset(drw, statusbigfont);
	else if (code[0] == 'N' && code[1] == '^')
		drw_setfontset(drw, normalfont);
}

void
tag(const Arg *arg)
{
	if (!selmon->sel || !(arg->ui & TAGMASK))
		return;

	if (mons->next) {
		if (!(arg->ui & SCREEN_MASK) && selmon != mons)
			/* moving to even tag, selected mon != first mon */
			selmon->sel->tags = arg->ui & TAGMASK;
		else if ((arg->ui & SCREEN_MASK) && selmon == mons)
			/* moving to odd tag, selected mon == first mon */
			selmon->sel->tags = arg->ui & TAGMASK;
		else {
			tagnextmon(arg);
			return;
		}
	} else
		selmon->sel->tags = arg->ui & TAGMASK;

	focus(NULL);
	arrange(selmon);
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tagmonview(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmonview(selmon->sel, dirtomon(arg->i));
}

void
tagnewmon(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
		view(arg);
	}
}

void
tagnextmon(const Arg *arg)
{
	Client *sel;
	Monitor *newmon;

	if (!(sel = selmon->sel) || !mons->next)
		return;
	newmon = dirtomon(1);
	sendmon(sel, newmon);
	if (arg->ui & TAGMASK) {
		sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(newmon);
	}
}

void
tagnthmonview(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmonview(selmon->sel, numtomon(arg->i));
}

void
tagview(const Arg *arg)
{
	if (!selmon->sel || !(arg->ui & TAGMASK))
		return;
	if (mons->next) {
		if (!(arg->ui & SCREEN_MASK) && selmon == mons) {
			/* first monitor and moving to even tag (second mon) */
			tagnthmonview(&(Arg){ .i = 1 });
			tagnewmon(arg);
			return;
		} else if ((arg->ui & SCREEN_MASK) && selmon != mons) {
			tagnthmonview(&(Arg){ .i = 0 });
			tagnewmon(arg);
			return;
		}
	}
	selmon->sel->tags = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
	view(arg);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	arrange(selmon);
}

void
togglebars(const Arg *arg)
{
	Monitor *m;
	int showbar = !selmon->showbar; /* keep all bars in sync */

	for (m = mons; m; m = m->next) {
		m->showbar = showbar;
		updatebarpos(m);
		XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
		arrange(m);
	}
}

/* Toggle between one layout for every tag and monitor and a layout per
 * tag (layoutalltags). */
void
togglelayoutalltags(const Arg *arg)
{
	layoutalltags = !layoutalltags;
	if (layoutalltags) /* every tag takes the current layout */
		setlayout(&((Arg) { .v = selmon->lt[selmon->sellt] }));
	notifysend("layout", layoutalltags ? "layout: all tags" : "layout: per tag");
}

void
togglefloating(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!c)
		return;
	if (c->isfullscreen) /* no support for fullscreen windows */
		return;
	c->isfloating = !c->isfloating || c->isfixed;
	if (c->isfloating) {
		/* center if never floated, or if the stored geometry is on
		 * another monitor (the client was moved since) */
		if (c->sfx == 0 || recttomon(c->sfx, c->sfy, c->sfw, c->sfh) != c->mon) {
			c->sfx = c->mon->mx + (c->mon->mw - c->sfw - 2 * c->bw) / 2;
			c->sfy = c->mon->my + (c->mon->mh - c->sfh - 2 * c->bw) / 2;
		}
		/* restore last known float dimensions */
		resize(c, c->sfx, c->sfy, c->sfw, c->sfh, 0);
	} else {
		/* save last known float dimensions */
		c->sfx = c->x;
		c->sfy = c->y;
		c->sfw = c->w;
		c->sfh = c->h;
	}
	arrange(selmon);
}

void
togglefullscr(const Arg *arg)
{
	if (selmon->sel)
		setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void
togglescratch(const Arg *arg)
{
	Client *c;
	unsigned int found = 0, scratchtag, newtagset;
	Arg sparg;

	if (arg->ui >= LENGTH(scratchpads))
		return;
	scratchtag = SPTAG(arg->ui);
	sparg.v = scratchpads[arg->ui].cmd;

	for (c = selmon->clients; c && !(found = c->tags & scratchtag); c = c->next);
	if (found) {
		newtagset = selmon->tagset[selmon->seltags] ^ scratchtag;
		if (newtagset) {
			selmon->tagset[selmon->seltags] = newtagset;
			focus(NULL);
			arrange(selmon);
		}
		if (ISVISIBLE(c)) {
			focus(c);
			restack(selmon);
		}
	} else {
		selmon->tagset[selmon->seltags] |= scratchtag;
		spawn(&sparg);
	}
}

void
togglesticky(const Arg *arg)
{
	if (!selmon->sel)
		return;
	setsticky(selmon->sel, !selmon->sel->issticky);
	notifysend("sticky", selmon->sel->issticky ? "sticky: on" : "sticky: off");
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	Client *s;
	XWindowChanges wc;

	if (c->swallowing) {
		unswallow(c);
		return;
	}

	if ((s = swallowingclient(c->win))) {
		/* c is the client s swallowed (in no list), freed as the patch does */
		free(s->swallowing);
		s->swallowing = NULL;
		arrange(m);
		focus(NULL);
		return;
	}

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	/* dwm's class, which scripts and compositor rules look the bar up by */
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	int i, j, n, nn = 0;
	Client *c, *next;
	Monitor *m;
	XineramaScreenInfo *info = NULL;

	if (XineramaIsActive(dpy))
		info = XineramaQueryScreens(dpy, &nn);
	/* only consider unique geometries as separate screens; they are
	 * gathered at the front of info, which needs no allocation */
	for (i = 0, j = 0; info && i < nn; i++)
		if (isuniquegeom(info, j, &info[i]))
			info[j++] = info[i];
	nn = j;
	/* dwm would dereference a NULL monitor here when there are no screens;
	 * treat it as no Xinerama */
	if (nn > 0) {
		for (n = 0, m = mons; m; m = m->next, n++);
		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		/* Logic for moving clients: only when monitors were added.
		 * Odd tags live on the first monitor and even tags on the second,
		 * so with two or more monitors move clients that have only even
		 * tags over. Like view() and tag(), a client with any odd tag
		 * (e.g. one on all tags) stays on the first monitor, and so do
		 * scratchpads, which have no normal tag bit. */
		if (nn > n && nn >= 2) {
			for (c = mons->clients; c; c = next) {
				next = c->next;
				/* Check if the client belongs to the second monitor */
				if ((c->tags & TAGBITS) && !(c->tags & SCREEN_MASK)) {
					detach(c); /* Detach from primary monitor */
					detachstack(c);
					c->mon = mons->next; /* Assign to secondary monitor */
					attach(c); /* Attach to secondary monitor */
					attachstack(c);
				}
			}
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			|| info[i].x_org != m->mx || info[i].y_org != m->my
			|| info[i].width != m->mw || info[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = info[i].x_org;
				m->my = m->wy = info[i].y_org;
				m->mw = m->ww = info[i].width;
				m->mh = m->wh = info[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attach(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
	}
	if (info)
		XFree(info);
	if (nn == 0)
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

/* Write _NET_WM_STATE as the list of the states dwm manages, so that
 * setting one state does not clear the other. */
void
updatenetwmstate(Client *c)
{
	Atom state[2] = { None, None };
	int n = 0;

	if (c->isfullscreen)
		state[n++] = netatom[NetWMFullscreen];
	if (c->issticky)
		state[n++] = netatom[NetWMSticky];
	XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)state, n);
}

void
updatenumlockmask(void)
{
	int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	if (!(modmap = XGetModifierMapping(dpy)))
		return;
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		snprintf(stext, sizeof stext, "%s", "dwmc-"VERSION);
	drawbars();
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		memcpy(c->name, broken, sizeof broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (state == netatom[NetWMSticky])
		setsticky(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	if (mons->next) {
		/* odd tags on first mon, even tags on second. Focus the target
		 * mon first so the check below uses its tagset. Arg {0}
		 * (previous tagset) stays on the current mon. */
		if (arg->ui & TAGMASK)
			focusnthmon(&(Arg){ .i = (arg->ui & SCREEN_MASK) ? 0 : 1 });
		if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
			return;
	} else if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) {
		/* the key of the current tag goes back to the previous tagset */
		view(&(Arg){ .ui = 0 });
		return;
	}

	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
}

/* The pid of the client owning w, from the X-Resource extension; 0 when it
 * is unknown. */
pid_t
winpid(Window w)
{
	pid_t result = 0;
	XResClientIdSpec spec = { w, XRES_CLIENT_ID_PID_MASK };
	XResClientIdValue *ids = NULL;
	long i, num_ids = 0;
	int evbase, errbase;
	Status status;

	/* the extension check avoids Xlib's "extension missing" message */
	if (!XResQueryExtension(dpy, &evbase, &errbase))
		return 0;
	/* drop any error of the request (the xcb version of the patch frees it) */
	XSetErrorHandler(xerrordummy);
	status = XResQueryClientIds(dpy, 1, &spec, &num_ids, &ids);
	XSetErrorHandler(xerror);
	if (status != Success || !ids)
		return 0;
	for (i = 0; i < num_ids; i++)
		if (ids[i].spec.mask & XRES_CLIENT_ID_PID_MASK) {
			result = XResGetClientPid(&ids[i]);
			break;
		}
	XResClientIdsDestroy(num_ids, ids);

	if (result == (pid_t)-1)
		result = 0;
	return result;
}

pid_t
getparentprocess(pid_t p)
{
	char buf[256], path[32], *s, *end;
	ssize_t n;
	long v;
	int fd;

	snprintf(path, sizeof path, "/proc/%u/stat", (unsigned)p);
	if ((fd = open(path, O_RDONLY)) < 0)
		return 0;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	/* "pid (comm) state ppid ...": comm may contain spaces and ')', and
	 * is at most 15 bytes, so the last ')' in the buffer ends it */
	if (!(s = strrchr(buf, ')')))
		return 0;
	s++;
	s += strspn(s, " \t\n\f\r"); /* the state */
	s += strcspn(s, " \t\n\f\r");
	v = strtol(s, &end, 10);
	if (end == s || (*end && !strchr(" \t\n\f\r", *end)) || v < INT_MIN || v > INT_MAX)
		return 0;
	return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
	int depth;

	/* bounded, so a parent chain garbled by PID reuse cannot loop forever */
	for (depth = 0; p != c && c != 0 && depth < 4096; depth++)
		c = getparentprocess(c);

	return c != 0 && p == c;
}

Client *
termforwin(const Client *w)
{
	Client *c;
	Monitor *m;

	if (!w->pid || w->isterminal)
		return NULL;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
				return c;
		}
	}

	return NULL;
}

Client *
swallowingclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->swallowing && c->swallowing->win == w)
				return c;
		}
	}

	return NULL;
}

/* Color for the ^2^ weather block, from the temperature that follows the
 * code in the status text (e.g. "+7°"): +20 and above is hot, below zero
 * is cold */
Clr *
weathercolor(const char *s)
{
	for (; *s && *s != '^' && (unsigned char)*s >= ' '; s++) {
		if (*s == '+')
			return &statusclr[strtol(s + 1, NULL, 10) >= 20 ? Col21 : Col22];
		if (*s == '-')
			return &statusclr[Col23];
		if (*s >= '0' && *s <= '9')
			break;
	}
	return &statusclr[Col24];
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwmc: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib ? xerrorxlib(dpy, ee) : 0; /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwmc: another window manager is already running");
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!c || !selmon->lt[selmon->sellt]->arrange || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

/* dwmr restores SIGPIPE to SIG_DFL here, since Rust ignores it; a C program
 * starts with the default disposition. */
int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwmc-"VERSION);
	else if (argc != 1)
		die("usage: dwmc [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwmc: cannot open display");
	checkotherwm();
	XrmInitialize();
	load_xresources();
	setup();
	scan();
	runautostart();
	arrange(selmon);
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
