/* See LICENSE file for copyright and license details. */
/* Mirrors the #[cfg(test)] modules of dwmr/src/util.rs and dwmr/src/drw.rs */
/* Not ported: the tests of dwmr's atoi/strtoul/strtof, which are libc here,
 * and of the TOML config loader. That config.def.h and config/config.h build
 * is what make test checks after running this. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "util.c"
#include "drw.c"

static void
truncate_keeps_char_boundaries(void)
{
	char s[5], t[11];

	truncate_utf8(s, "aé€😀", sizeof s); /* 'a' (1) + 'é' (2) = 3, '€' would need 3 more */
	assert(!strcmp(s, "aé"));
	truncate_utf8(t, "abc", sizeof t);
	assert(!strcmp(t, "abc"));
}

static void
between_is_inclusive(void)
{
	assert(BETWEEN(1, 1, 3));
	assert(BETWEEN(3, 1, 3));
	assert(!BETWEEN(4, 1, 3));
}

static void
utf8decode_ascii_and_multibyte(void)
{
	long u;
	int err;

	assert(utf8decode("a", &u, &err) == 1 && u == 'a' && !err);
	assert(utf8decode("é", &u, &err) == 2 && u == 0xE9 && !err);
	assert(utf8decode("€", &u, &err) == 3 && u == 0x20AC && !err);
	assert(utf8decode("😀", &u, &err) == 4 && u == 0x1F600 && !err);
}

static void
utf8decode_invalid(void)
{
	long u;
	int err;

	/* lone continuation byte */
	assert(utf8decode("\x80", &u, &err) == 1 && u == UTF_INVALID && err);
	/* truncated sequence */
	assert(utf8decode("\xE2\x82", &u, &err) == 2 && u == UTF_INVALID && err);
	/* overlong encoding of '/' */
	assert(utf8decode("\xC0\xAF", &u, &err) == 2 && u == UTF_INVALID && err);
	/* surrogate */
	assert(utf8decode("\xED\xA0\x80", &u, &err) == 3 && u == UTF_INVALID && err);
	/* (the Rust test's empty input does not exist here: drw_text() never
	 * decodes at the terminating NUL) */
}

static void
shortentitle_cuts_at_words(void)
{
	char buf[264], src[300];
	int i;

	assert(shortentitle(buf, "diegoMalagrida/pocket-dots: My Arch Linux dotfiles, ready to install on a fresh system. - Mozilla Firefox", sizeof buf, 60, 20));
	assert(!strcmp(buf, "diegoMalagrida/pocket-dots: My Arch Linux dotfiles, ready to … - Mozilla Firefox"));
	/* the separator wins over the space closer to tail ("baz — Mozilla Firefox") */
	assert(shortentitle(buf, "Some very long page title that keeps going and going for a good while, foo bar baz — Mozilla Firefox", sizeof buf, 60, 20));
	assert(!strcmp(buf, "Some very long page title that keeps going and going for a … — Mozilla Firefox"));
	/* up to head + tail + 18 characters are shown as they are */
	memset(src, 'a', 50);
	src[50] = ' ';
	memset(src + 51, 'b', 47);
	src[98] = '\0';
	assert(!shortentitle(buf, src, sizeof buf, 60, 20));
	src[98] = 'b';
	src[99] = '\0';
	assert(shortentitle(buf, src, sizeof buf, 60, 20));
	/* no space in range: exactly head / tail characters, not bytes */
	for (i = 0; i < 120; i++)
		memcpy(src + 2 * i, "é", 2);
	src[240] = '\0';
	assert(shortentitle(buf, src, sizeof buf, 60, 20));
	assert(!strncmp(buf, src, 120) && !strcmp(buf + 120, " … éééééééééééééééééééé"));
	assert(!shortentitle(buf, "diegoMalagrida/pocket-dots: My Arch Linux dotfiles, ready to install on a fresh system. - Mozilla Firefox", sizeof buf, 0, 20));
	assert(!shortentitle(buf, "diegoMalagrida/pocket-dots: My Arch Linux dotfiles, ready to install on a fresh system. - Mozilla Firefox", sizeof buf, 60, 256));
}

int
main(void)
{
	truncate_keeps_char_boundaries();
	shortentitle_cuts_at_words();
	between_is_inclusive();
	utf8decode_ascii_and_multibyte();
	utf8decode_invalid();
	puts("test: all passed");
	return 0;
}
