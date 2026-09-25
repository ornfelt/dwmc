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

/* A layout size computed from mfact, converted to int. Not in the patch:
 * mfact may be any float config.h or X resources set, and converting an
 * out-of-range float to int is undefined in C. NaN gives 0 and the rest
 * saturates, as dwmr's `as i32` does, but at half the int limits, so that
 * the int arithmetic after it stays defined (dwmr wraps there). */
static int
ftoi(float f)
{
	if (f != f)
		return 0;
	if (f >= (float)(INT_MAX / 2))
		return INT_MAX / 2;
	if (f <= (float)(INT_MIN / 2))
		return INT_MIN / 2;
	return f;
}

void
setgaps(int oh, int ov, int ih, int iv)
{
	if (oh < 0) oh = 0;
	if (ov < 0) ov = 0;
	if (ih < 0) ih = 0;
	if (iv < 0) iv = 0;

	selmon->gappoh = MIN(oh, GAP_MAX);
	selmon->gappov = MIN(ov, GAP_MAX);
	selmon->gappih = MIN(ih, GAP_MAX);
	selmon->gappiv = MIN(iv, GAP_MAX);
	arrange(selmon);
}

void
togglegaps(const Arg *arg)
{
	enablegaps = !enablegaps;
	arrange(NULL);
}

void
togglebgaps(const Arg *arg)
{
	browsergaps = !browsergaps;
	arrange(NULL);
}

void
defaultgaps(const Arg *arg)
{
	setgaps(gappoh, gappov, gappih, gappiv);
}

void
incrgaps(const Arg *arg)
{
	/* the gaps are within 0..GAP_MAX, so limiting the step to GAP_MAX gives
	 * what dwmr's saturating_add gives after setgaps() clamps, without
	 * overflowing */
	int i = MAX(MIN(arg->i, GAP_MAX), -GAP_MAX);

	setgaps(
		selmon->gappoh + i,
		selmon->gappov + i,
		selmon->gappih + i,
		selmon->gappiv + i
	);
}

void
getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, int *nc)
{
	int n, oe, ie;
	Client *c;

	oe = ie = enablegaps;
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (smartgaps && n == 1) {
		oe = 0; /* outer gaps disabled when only one client */
	}

	if (n == 1 && nexttiled(m->clients)->isbrowser && !browsergaps) {
		oe = 0; /* outer gaps disabled when only one client (and it's Firefox) */
	}

	*oh = oe ? m->gappoh : 0; /* outer horizontal gap */
	*ov = oe ? m->gappov : 0; /* outer vertical gap */
	*ih = ie ? m->gappih : 0; /* inner horizontal gap */
	*iv = ie ? m->gappiv : 0; /* inner vertical gap */
	*nc = n;                  /* number of clients */
}

/* getfacts() without cfacts: every client weighs 1, so each master client
 * gets msize / mfacts and each stack client ssize / sfacts. Returns those
 * shares in mf and sf instead of the facts (so the layouts never divide),
 * plus the remainders. */
void
getfacts(Monitor *m, int msize, int ssize, int *mf, int *sf, int *mr, int *sr)
{
	int n, mfacts = 0, sfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		if (n < m->nmaster)
			mfacts++;
		else
			sfacts++;

	*mf = mfacts ? msize / mfacts : 0; /* size of a master client */
	*sf = sfacts ? ssize / sfacts : 0; /* size of a stack client */
	*mr = msize - *mf * mfacts; /* the remainder (rest) of pixels after a master split */
	*sr = ssize - *sf * sfacts; /* the remainder (rest) of pixels after a stack split */
}

/* The layouts clamp nmaster to 0..n, for n > 0 tiled clients. Not in the
 * patch: it changes no layout (i < nmaster and n > nmaster test the same)
 * but keeps the gap arithmetic in range for any nmaster. */

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
	int i, n, nmaster;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	int mf, sf, mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nmaster = MIN(MAX(m->nmaster, 0), n);
	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh;
	mw = m->ww - 2*ov - iv * (MIN(n, nmaster) - 1);
	sw = m->ww - 2*ov - iv * (n - nmaster - 1);

	if (nmaster && n > nmaster) {
		sh = ftoi((mh - ih) * (1 - m->mfact));
		mh = mh - ih - sh;
		sx = mx;
		sy = my + mh + ih;
	}

	getfacts(m, mw, sw, &mf, &sf, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (i < nmaster) {
			resize(c, mx, my, mf + (i < mrest) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv;
		} else {
			resize(c, sx, sy, sf + ((i - nmaster) < srest) - (2*c->bw), sh - (2*c->bw), 0);
			sx += WIDTH(c) + iv;
		}
	}
}

/*
 * Centred master layout + gaps
 * https://dwm.suckless.org/patches/centeredmaster/
 */
void
centeredmaster(Monitor *m)
{
	int i, n, nmaster;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int lx = 0, ly = 0, lw = 0, lh = 0;
	int rx = 0, ry = 0, rw = 0, rh = 0;
	int mfacts = 0, lfacts = 0, rfacts = 0;
	int mf, lf, rf;
	int mrest = 0, lrest = 0, rrest = 0;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nmaster = MIN(MAX(m->nmaster, 0), n);

	/* initialize areas */
	mx = m->wx + ov;
	my = m->wy + oh;
	mh = m->wh - 2*oh - ih * ((!nmaster ? n : MIN(n, nmaster)) - 1);
	mw = m->ww - 2*ov;
	lh = m->wh - 2*oh - ih * (((n - nmaster) / 2) - 1);
	rh = m->wh - 2*oh - ih * (((n - nmaster) / 2) - ((n - nmaster) % 2 ? 0 : 1));

	if (nmaster && n > nmaster) {
		/* go mfact box in the center if more than nmaster clients */
		if (n - nmaster > 1) {
			/* ||<-S->|<---M--->|<-S->|| */
			mw = ftoi((m->ww - 2*ov - 2*iv) * m->mfact);
			lw = (m->ww - mw - 2*ov - 2*iv) / 2;
			rw = (m->ww - mw - 2*ov - 2*iv) - lw;
			mx += lw + iv;
		} else {
			/* ||<---M--->|<-S->|| */
			mw = ftoi((mw - iv) * m->mfact);
			lw = 0;
			rw = m->ww - mw - iv - 2*ov;
		}
		lx = m->wx + ov;
		ly = m->wy + oh;
		rx = mx + mw + iv;
		ry = m->wy + oh;
	}

	/* calculate facts: every client weighs 1 */
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (!nmaster || n < nmaster)
			mfacts++;
		else if ((n - nmaster) % 2)
			lfacts++; /* total factor of left hand stack area */
		else
			rfacts++; /* total factor of right hand stack area */
	}

	mf = mfacts ? mh / mfacts : 0;
	lf = lfacts ? lh / lfacts : 0;
	rf = rfacts ? rh / rfacts : 0;
	mrest = mh - mf * mfacts;
	lrest = lh - lf * lfacts;
	rrest = rh - rf * rfacts;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (!nmaster || i < nmaster) {
			/* nmaster clients are stacked vertically, in the center of the screen */
			resize(c, mx, my, mw - (2*c->bw), mf + (i < mrest) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			/* stack clients are stacked vertically; the patch tests
			 * (i - 2*nmaster) < 2*rest in unsigned arithmetic, which skips
			 * the first clients of a side and loses the rest pixels when
			 * nmaster > 2, so the first rest clients of each side get one
			 * pixel more here: (i - nmaster) / 2 is the index on its side */
			if ((i - nmaster) % 2) {
				resize(c, lx, ly, lw - (2*c->bw), lf + ((i - nmaster) / 2 < lrest) - (2*c->bw), 0);
				ly += HEIGHT(c) + ih;
			} else {
				resize(c, rx, ry, rw - (2*c->bw), rf + ((i - nmaster) / 2 < rrest) - (2*c->bw), 0);
				ry += HEIGHT(c) + ih;
			}
		}
	}
}

void
centeredfloatingmaster(Monitor *m)
{
	int i, n, nmaster;
	float mivf = 1.0; /* master inner vertical gap factor */
	int oh, ov, ih, iv, mf, sf, mrest, srest;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nmaster = MIN(MAX(m->nmaster, 0), n);
	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh;
	mw = m->ww - 2*ov - iv*(n - 1);
	sw = m->ww - 2*ov - iv*(n - nmaster - 1);

	if (nmaster && n > nmaster) {
		mivf = 0.8;
		/* go mfact box in the center if more than nmaster clients */
		if (m->ww > m->wh) {
			mw = ftoi(m->ww * m->mfact - iv*mivf*(MIN(n, nmaster) - 1));
			mh = m->wh * 0.9;
		} else {
			mw = m->ww * 0.9 - iv*mivf*(MIN(n, nmaster) - 1);
			mh = ftoi(m->wh * m->mfact);
		}
		mx = m->wx + (m->ww - mw) / 2;
		my = m->wy + (m->wh - mh - 2*oh) / 2;

		sx = m->wx + ov;
		sy = m->wy + oh;
		sh = m->wh - 2*oh;
	}

	getfacts(m, mw, sw, &mf, &sf, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < nmaster) {
			/* nmaster clients are stacked horizontally, in the center of the screen */
			resize(c, mx, my, mf + (i < mrest) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv*mivf;
		} else {
			/* stack clients are stacked horizontally */
			resize(c, sx, sy, sf + ((i - nmaster) < srest) - (2*c->bw), sh - (2*c->bw), 0);
			sx += WIDTH(c) + iv;
		}
}

/*
 * Deck layout + gaps
 * https://dwm.suckless.org/patches/deck/
 */
void
deck(Monitor *m)
{
	int i, n, nmaster;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	int mf, sf, mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nmaster = MIN(MAX(m->nmaster, 0), n);
	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh - ih * (MIN(n, nmaster) - 1);
	sw = mw = m->ww - 2*ov;

	if (nmaster && n > nmaster) {
		sw = ftoi((mw - iv) * (1 - m->mfact));
		mw = mw - iv - sw;
		sx = mx + mw + iv;
		sh = m->wh - 2*oh;
	}

	getfacts(m, mh, sh, &mf, &sf, &mrest, &srest);

	/* override layout symbol; the patch tests n - nmaster > 0 unsigned,
	 * which shows "D -1" when there are fewer clients than nmaster */
	if (n - nmaster > 0)
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "D %d", n - nmaster);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < nmaster) {
			resize(c, mx, my, mw - (2*c->bw), mf + (i < mrest) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			resize(c, sx, sy, sw - (2*c->bw), sh - (2*c->bw), 0);
		}
}

/*
 * Fibonacci layout + gaps
 * https://dwm.suckless.org/patches/fibonacci/
 */
void
fibonacci(Monitor *m, int s)
{
	int i, n;
	int nx, ny, nw, nh;
	int oh, ov, ih, iv;
	int nv, hrest = 0, wrest = 0, r = 1;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nx = m->wx + ov;
	ny = m->wy + oh;
	nw = m->ww - 2*ov;
	nh = m->wh - 2*oh;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
		if (r) {
			if ((i % 2 && (nh - ih) / 2 <= (bh + 2*c->bw))
			   || (!(i % 2) && (nw - iv) / 2 <= (bh + 2*c->bw))) {
				r = 0;
			}
			if (r && i < n - 1) {
				if (i % 2) {
					nv = (nh - ih) / 2;
					hrest = nh - 2*nv - ih;
					nh = nv;
				} else {
					nv = (nw - iv) / 2;
					wrest = nw - 2*nv - iv;
					nw = nv;
				}

				if ((i % 4) == 2 && !s)
					nx += nw + iv;
				else if ((i % 4) == 3 && !s)
					ny += nh + ih;
			}

			if ((i % 4) == 0) {
				if (s) {
					ny += nh + ih;
					nh += hrest;
				}
				else {
					nh -= hrest;
					ny -= nh + ih;
				}
			}
			else if ((i % 4) == 1) {
				nx += nw + iv;
				nw += wrest;
			}
			else if ((i % 4) == 2) {
				ny += nh + ih;
				nh += hrest;
				if (i < n - 1)
					nw += wrest;
			}
			else if ((i % 4) == 3) {
				if (s) {
					nx += nw + iv;
					nw -= wrest;
				} else {
					nw -= wrest;
					nx -= nw + iv;
					nh += hrest;
				}
			}
			if (i == 0)	{
				if (n != 1) {
					nw = ftoi((m->ww - iv - 2*ov) - (m->ww - iv - 2*ov) * (1 - m->mfact));
					wrest = 0;
				}
				ny = m->wy + oh;
			}
			else if (i == 1)
				nw = m->ww - nw - iv - 2*ov;
			i++;
		}

		resize(c, nx, ny, nw - (2*c->bw), nh - (2*c->bw), 0);
	}
}

void
dwindle(Monitor *m)
{
	fibonacci(m, 1);
}

void
spiral(Monitor *m)
{
	fibonacci(m, 0);
}

/*
 * Default tile layout + gaps
 */
void
tile(Monitor *m)
{
	int i, n, nmaster;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	int mf, sf, mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nmaster = MIN(MAX(m->nmaster, 0), n);
	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	mh = m->wh - 2*oh - ih * (MIN(n, nmaster) - 1);
	sh = m->wh - 2*oh - ih * (n - nmaster - 1);
	sw = mw = m->ww - 2*ov;

	if (nmaster && n > nmaster) {
		sw = ftoi((mw - iv) * (1 - m->mfact));
		mw = mw - iv - sw;
		sx = mx + mw + iv;
	}

	getfacts(m, mh, sh, &mf, &sf, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < nmaster) {
			resize(c, mx, my, mw - (2*c->bw), mf + (i < mrest) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			resize(c, sx, sy, sw - (2*c->bw), sf + ((i - nmaster) < srest) - (2*c->bw), 0);
			sy += HEIGHT(c) + ih;
		}
}
