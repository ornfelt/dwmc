# dwmc - dynamic window manager (C port of dwmr)
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}

# config.h is a copy of ~/.config/dwmc/config.h, or of config.def.h when
# there is none; under sudo, use the invoking user's config, not root's
USERHOME = ${HOME}
ifneq (${SUDO_USER},)
USERHOME = $(shell getent passwd ${SUDO_USER} | cut -d: -f6)
endif
CONFDIR = ${USERHOME}/.config/dwmc

all: dwmc

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk drw.h util.h

dwm.o: vanitygaps.c

config.h: config.def.h $(wildcard ${CONFDIR}/config.h)
	if [ -e ${CONFDIR}/config.h ]; then cp ${CONFDIR}/config.h $@; else cp config.def.h $@; fi

dwmc: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

# dwmc with the sanitizers, for the functional tests
debug: dwmc-debug

dwmc-debug: ${SRC} vanitygaps.c config.h config.mk drw.h util.h
	${CC} ${CFLAGS} ${DEBUGCFLAGS} -o $@ ${SRC} ${LDFLAGS}

# the unit tests, then dwm.c built against both configs: config.def.h and
# the shipped config/config.h (in test.d, whose config.h dwm.c then includes)
test: test.c util.c drw.c dwm.c vanitygaps.c drw.h util.h config.def.h config/config.h config.mk
	${CC} ${CFLAGS} ${DEBUGCFLAGS} -o test test.c ${LDFLAGS}
	./test
	mkdir -p test.d
	ln -sf ../dwm.c test.d/dwm.c
	for c in config.def.h config/config.h; do \
		cp $$c test.d/config.h && \
		${CC} -c ${CFLAGS} -I. -o test.d/dwm.o test.d/dwm.c || exit 1; \
	done

transient: transient.c
	${CC} ${CFLAGS} -o $@ transient.c -L${X11LIB} -lX11

clean:
	rm -f dwmc dwmc-debug test transient ${OBJ} config.h dwmc-${VERSION}.tar.gz
	rm -rf test.d

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwmc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwmc
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwmc.1 > ${DESTDIR}${MANPREFIX}/man1/dwmc.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwmc.1

# copy the shipped config to ~/.config/dwmc unless one is already there
install-config:
	mkdir -p ${CONFDIR}
	[ -e ${CONFDIR}/config.h ] || cp config/config.h ${CONFDIR}/config.h

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwmc\
		${DESTDIR}${MANPREFIX}/man1/dwmc.1

.PHONY: all debug test clean install install-config uninstall
