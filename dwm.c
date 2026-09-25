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
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
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
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatusbar(Monitor *m, int bh, const char *stext);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusnthmon(const Arg *arg);
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
static void load_xresources(void);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
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
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static unsigned int shifttags(unsigned int tags, int i);
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
static void getfacts(Monitor *m, int msize, int ssize, int *mf, int *sf, int *mr, int *sr);
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
	/* TODO: port body - dwm.rs -> Dwm::applyrules */
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	/* TODO: port body - dwm.rs -> Dwm::applysizehints */
	return 0;
}

void
arrange(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::arrange */
}

void
arrangemon(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::arrangemon */
}

void
attach(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::attach */
}

void
attachstack(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::attachstack */
}

void
swallow(Client *p, Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::swallow */
}

void
unswallow(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::unswallow */
}

void
buttonpress(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::buttonpress */
}

void
checkotherwm(void)
{
	/* TODO: port body - dwm.rs -> Dwm::checkotherwm */
}

void
cleanup(void)
{
	/* TODO: port body - dwm.rs -> Dwm::cleanup */
}

void
cleanupmon(Monitor *mon)
{
	/* TODO: port body - dwm.rs -> Dwm::cleanupmon */
}

void
clientmessage(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::clientmessage */
}

void
configure(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::configure */
}

void
configurenotify(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::configurenotify */
}

void
configurerequest(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::configurerequest */
}

Monitor *
createmon(void)
{
	/* TODO: port body - dwm.rs -> Dwm::createmon */
	return NULL;
}

void
destroynotify(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::destroynotify */
}

void
detach(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::detach */
}

void
detachstack(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::detachstack */
}

Monitor *
dirtomon(int dir)
{
	/* TODO: port body - dwm.rs -> Dwm::dirtomon */
	return NULL;
}

void
drawbar(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::drawbar */
}

void
drawbars(void)
{
	/* TODO: port body - dwm.rs -> Dwm::drawbars */
}

int
drawstatusbar(Monitor *m, int bh, const char *stext)
{
	/* TODO: port body - dwm.rs -> Dwm::drawstatusbar */
	return 0;
}

void
expose(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::expose */
}

void
focus(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::focus */
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::focusin */
}

void
focusmon(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::focusmon */
}

void
focusnthmon(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::focusnthmon */
}

void
focusstack(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::focusstack */
}

Atom
getatomprop(Client *c, Atom prop)
{
	/* TODO: port body - dwm.rs -> Dwm::getatomprop */
	return None;
}

int
getrootptr(int *x, int *y)
{
	/* TODO: port body - dwm.rs -> Dwm::getrootptr */
	return 0;
}

long
getstate(Window w)
{
	/* TODO: port body - dwm.rs -> Dwm::getstate */
	return -1;
}

pid_t
getstatusbarpid(void)
{
	/* TODO: port body - dwm.rs -> Dwm::getstatusbarpid */
	return -1;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	/* TODO: port body - dwm.rs -> Dwm::gettextprop (+ Dwm::copy_text) */
	return 0;
}

void
grabbuttons(Client *c, int focused)
{
	/* TODO: port body - dwm.rs -> Dwm::grabbuttons */
}

void
grabkeys(void)
{
	/* TODO: port body - dwm.rs -> Dwm::grabkeys */
}

void
incnmaster(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::incnmaster */
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	/* TODO: port body - dwm.rs -> isuniquegeom */
	return 0;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::keypress */
}

void
killclient(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::killclient */
}

void
load_xresources(void)
{
	/* TODO: port body - dwm.rs -> Dwm::load_xresources */
}

void
manage(Window w, XWindowAttributes *wa)
{
	/* TODO: port body - dwm.rs -> Dwm::manage */
}

void
mappingnotify(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::mappingnotify */
}

void
maprequest(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::maprequest */
}

void
monocle(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::monocle */
}

void
movemouse(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::movemouse */
}

Client *
nexttiled(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::nexttiled */
	return NULL;
}

Monitor *
numtomon(int num)
{
	/* TODO: port body - dwm.rs -> Dwm::numtomon */
	return NULL;
}

void
pop(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::pop */
}

void
propertynotify(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::propertynotify */
}

void
pushstack(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::pushstack */
}

void
quit(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::quit */
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	/* TODO: port body - dwm.rs -> Dwm::recttomon */
	return NULL;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	/* TODO: port body - dwm.rs -> Dwm::resize */
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	/* TODO: port body - dwm.rs -> Dwm::resizeclient */
}

void
resizemouse(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::resizemouse */
}

void
resource_load(XrmDatabase db, const char *name, enum resource_type rtype, void *dst)
{
	/* TODO: port body - dwm.rs -> Dwm::resource_load */
}

void
restack(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::restack */
}

void
run(void)
{
	/* TODO: port body - dwm.rs -> Dwm::run */
}

void
runautostart(void)
{
	/* TODO: port body - dwm.rs -> Dwm::runautostart */
}

void
scan(void)
{
	/* TODO: port body - dwm.rs -> Dwm::scan */
}

void
sendmon(Client *c, Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::sendmon */
}

void
sendmonview(Client *c, Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::sendmonview */
}

void
setclientstate(Client *c, long state)
{
	/* TODO: port body - dwm.rs -> Dwm::setclientstate */
}

int
sendevent(Client *c, Atom proto)
{
	/* TODO: port body - dwm.rs -> Dwm::sendevent */
	return 0;
}

void
setfocus(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::setfocus */
}

void
setfullscreen(Client *c, int fullscreen)
{
	/* TODO: port body - dwm.rs -> Dwm::setfullscreen */
}

void
setsticky(Client *c, int sticky)
{
	/* TODO: port body - dwm.rs -> Dwm::setsticky */
}

void
setlayout(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::setlayout */
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::setmfact */
}

void
setup(void)
{
	/* TODO: port body - dwm.rs -> Dwm::setup (+ the init screen part of Dwm::new) */
}

void
seturgent(Client *c, int urg)
{
	/* TODO: port body - dwm.rs -> Dwm::seturgent */
}

unsigned int
shifttags(unsigned int tags, int i)
{
	/* TODO: port body - dwm.rs -> Dwm::shifttags */
	return 0;
}

/* Sends a window to the next/prev tag */
void
shifttag(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::shifttag */
}

/* Navigate to the next/prev tag */
void
shiftview(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::shiftview */
}

/* Navigate to the next/prev tag that has a client, else moves it to the next/prev tag */
void
shiftviewclients(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::shiftviewclients */
}

void
showhide(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::showhide */
}

void
sigstatusbar(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::sigstatusbar */
}

void
spawn(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::spawn */
}

int
stackpos(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::stackpos */
	return -1;
}

Clr *
statuscolor(const char *hex)
{
	/* TODO: port body - dwm.rs -> Dwm::statuscolor */
	return NULL;
}

void
statusfontcode(const char *code)
{
	/* TODO: port body - dwm.rs -> Dwm::statusfontcode */
}

void
tag(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tag */
}

void
tagmon(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagmon */
}

void
tagmonview(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagmonview */
}

void
tagnewmon(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagnewmon */
}

void
tagnextmon(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagnextmon */
}

void
tagnthmonview(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagnthmonview */
}

void
tagview(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::tagview */
}

void
togglebar(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglebar */
}

void
togglebars(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglebars */
}

void
togglefloating(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglefloating */
}

void
togglefullscr(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglefullscr */
}

void
togglescratch(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglescratch */
}

void
togglesticky(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::togglesticky */
}

void
toggletag(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::toggletag */
}

void
toggleview(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::toggleview */
}

void
unfocus(Client *c, int setfocus)
{
	/* TODO: port body - dwm.rs -> Dwm::unfocus */
}

void
unmanage(Client *c, int destroyed)
{
	/* TODO: port body - dwm.rs -> Dwm::unmanage */
}

void
unmapnotify(XEvent *e)
{
	/* TODO: port body - dwm.rs -> Dwm::unmapnotify */
}

void
updatebars(void)
{
	/* TODO: port body - dwm.rs -> Dwm::updatebars */
}

void
updatebarpos(Monitor *m)
{
	/* TODO: port body - dwm.rs -> Dwm::updatebarpos */
}

void
updateclientlist(void)
{
	/* TODO: port body - dwm.rs -> Dwm::updateclientlist */
}

int
updategeom(void)
{
	/* TODO: port body - dwm.rs -> Dwm::updategeom (+ Dwm::updategeom_xinerama) */
	return 0;
}

void
updatenetwmstate(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::updatenetwmstate */
}

void
updatenumlockmask(void)
{
	/* TODO: port body - dwm.rs -> Dwm::updatenumlockmask */
}

void
updatesizehints(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::updatesizehints */
}

void
updatestatus(void)
{
	/* TODO: port body - dwm.rs -> Dwm::updatestatus */
}

void
updatetitle(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::updatetitle */
}

void
updatewindowtype(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::updatewindowtype */
}

void
updatewmhints(Client *c)
{
	/* TODO: port body - dwm.rs -> Dwm::updatewmhints */
}

void
view(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::view */
}

pid_t
winpid(Window w)
{
	/* TODO: port body - dwm.rs -> Dwm::winpid */
	return 0;
}

pid_t
getparentprocess(pid_t p)
{
	/* TODO: port body - dwm.rs -> Dwm::getparentprocess */
	return 0;
}

int
isdescprocess(pid_t p, pid_t c)
{
	/* TODO: port body - dwm.rs -> Dwm::isdescprocess */
	return 0;
}

Client *
termforwin(const Client *w)
{
	/* TODO: port body - dwm.rs -> Dwm::termforwin */
	return NULL;
}

Client *
swallowingclient(Window w)
{
	/* TODO: port body - dwm.rs -> Dwm::swallowingclient */
	return NULL;
}

Clr *
weathercolor(const char *s)
{
	/* TODO: port body - dwm.rs -> Dwm::weathercolor */
	return NULL;
}

Client *
wintoclient(Window w)
{
	/* TODO: port body - dwm.rs -> Dwm::wintoclient */
	return NULL;
}

Monitor *
wintomon(Window w)
{
	/* TODO: port body - dwm.rs -> Dwm::wintomon */
	return NULL;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	/* TODO: port body - dwm.rs -> xerror */
	return 0;
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	/* TODO: port body - dwm.rs -> xerrordummy */
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	/* TODO: port body - dwm.rs -> xerrorstart */
	return 0;
}

void
zoom(const Arg *arg)
{
	/* TODO: port body - dwm.rs -> Dwm::zoom */
}

int
main(int argc, char *argv[])
{
	/* TODO: port body - main.rs -> main */
	return EXIT_SUCCESS;
}
