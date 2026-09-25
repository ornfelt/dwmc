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
