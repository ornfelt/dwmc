/* See LICENSE file for copyright and license details. */
/* Mirrors dwmr/src/util.rs */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* Print the message to stderr and exit with status 1. If it ends with a ':'
 * the description of errno is appended, the way dwmr prints it (Rust's
 * io::Error: "<strerror> (os error <errno>)"). */
void
die(const char *fmt, ...)
{
	va_list ap;
	int saved_errno;

	saved_errno = errno;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s (os error %d)", strerror(saved_errno), saved_errno);
	fputc('\n', stderr);

	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

/* Copy src into the buffer dst of size bytes, cut at a character boundary
 * when it does not fit, so no UTF-8 sequence is split. This mirrors the
 * fixed-size char[N] buffers dwm uses for window titles, the status text and
 * the layout symbol. */
void
truncate_utf8(char *dst, const char *src, size_t size)
{
	size_t n;

	if (!size)
		return;
	for (n = 0; n < size - 1 && src[n]; n++)
		dst[n] = src[n];
	if (src[n])
		while (n > 0 && (src[n] & 0xC0) == 0x80)
			n--;
	dst[n] = '\0';
}

#define ISBLANK(C)              ((C) == ' ' || (C) == '\t')
#define ISSEPARATOR(S)          (*(S) == '-' || *(S) == '|' || !strncmp((S), "\xe2\x80\x93", 3) \
                                 || !strncmp((S), "\xe2\x80\x94", 3)) /* - | – — */
#define DIST(A, B)              ((A) > (B) ? (A) - (B) : (B) - (A))

/* Shorten a title of more than head + tail + 18 characters to its start,
 * " … " and its end (shorttitle). The start is cut at the space closest to
 * head characters, within head - 5 .. head + 10; the end starts at an app
 * name separator (" - ", " – ", " — ", " | ") between tail - 5 and tail + 5
 * characters from the end, else at the space there closest to tail. With no
 * space in range a part is cut at exactly head / tail characters. Writes the
 * shortened title to dst (size bytes) and returns 1, or returns 0 when src
 * is short enough to be shown as it is (or head / tail is not 1..255). */
int
shortentitle(char *dst, const char *src, size_t size, unsigned int head, unsigned int tail)
{
	size_t off[257] = {0}, n, i, h, t, best, hmin, hmax, tmin, tmax;

	if (head < 1 || head > 255 || tail < 1 || tail > 255)
		return 0;
	/* the byte offset of each character, so lengths count characters */
	for (n = i = 0; src[i]; i++)
		if ((src[i] & 0xC0) != 0x80) {
			if (n == LENGTH(off) - 1)
				return 0;
			off[n++] = i;
		}
	off[n] = i;
	hmin = head > 5 ? head - 5 : 1;
	hmax = head + 10;
	tmin = tail > 5 ? tail - 5 : 1;
	tmax = tail + 5;
	if (n <= hmax + 3 + tmax)
		return 0;

	/* the start, up to the space closest to head */
	for (best = 0, i = hmin; i <= hmax; i++)
		if (ISBLANK(src[off[i]]) && (!best || DIST(i, head) < DIST(best, head)))
			best = i;
	h = best ? best : head;
	while (h > 0 && ISBLANK(src[off[h - 1]]))
		h--;

	/* the end, from the separator, else the space, closest to tail */
	for (best = 0, i = n - tmax; i <= n - tmin; i++)
		if (ISBLANK(src[off[i - 1]]) && i + 1 < n && ISBLANK(src[off[i + 1]])
		&& ISSEPARATOR(src + off[i])
		&& (!best || DIST(n - i, tail) < DIST(n - best, tail)))
			best = i;
	if (!best)
		for (i = n - tmax; i <= n - tmin; i++)
			if (ISBLANK(src[off[i - 1]])
			&& (!best || DIST(n - i, tail) < DIST(n - best, tail)))
				best = i;
	t = best ? best : n - tail;
	while (t < n && ISBLANK(src[off[t]]))
		t++;
	snprintf(dst, size, "%.*s \xe2\x80\xa6 %s", (int)off[h], src, src + off[t]);
	return 1;
}
