/* See LICENSE file for copyright and license details. */
/* Mirrors dwmr/src/vanitygaps.rs */
/* vanitygaps.c of dwm's cfacts-vanitygaps combo patch: gaps between windows
 * and the screen edge, and the gap-aware layouts. dwm.c #includes it after
 * config.h; its functions are declared with dwm.c's.
 *
 * cfacts is not ported: every client has the weight 1, so getfacts() splits
 * an area evenly. */

/* The largest gap setgaps() stores and createmon() takes from config.h. Not
 * in the patch: it keeps the gap arithmetic in the layouts far from
 * overflowing. */
#define GAP_MAX 1000

/* Settings */
static int enablegaps = 1;

void
setgaps(int oh, int ov, int ih, int iv)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::setgaps */
}

void
togglegaps(const Arg *arg)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::togglegaps */
}

void
togglebgaps(const Arg *arg)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::togglebgaps */
}

void
defaultgaps(const Arg *arg)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::defaultgaps */
}

void
incrgaps(const Arg *arg)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::incrgaps */
}

void
getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, int *nc)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::getgaps */
}

void
getfacts(Monitor *m, int msize, int ssize, int *mf, int *sf, int *mr, int *sr)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::getfacts (+ the nmaster clamp of Dwm::layoutarea) */
}

/***
 * Layouts
 */

/*
 * Bottomstack layout + gaps
 * https://dwm.suckless.org/patches/bottomstack/
 */
void
bstack(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::bstack */
}

/*
 * Centred master layout + gaps
 * https://dwm.suckless.org/patches/centeredmaster/
 */
void
centeredmaster(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::centeredmaster */
}

void
centeredfloatingmaster(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::centeredfloatingmaster */
}

/*
 * Deck layout + gaps
 * https://dwm.suckless.org/patches/deck/
 */
void
deck(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::deck */
}

/*
 * Fibonacci layout + gaps
 * https://dwm.suckless.org/patches/fibonacci/
 */
void
fibonacci(Monitor *m, int s)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::fibonacci */
}

void
dwindle(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::dwindle */
}

void
spiral(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::spiral */
}

/*
 * Default tile layout + gaps
 */
void
tile(Monitor *m)
{
	/* TODO: port body - vanitygaps.rs -> Dwm::tile */
}
