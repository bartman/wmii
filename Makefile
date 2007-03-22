# window manager improved 2 - window manager improved 2
#   © 2006 Anselm R. Garbe
#   © 2006-2007 Kris Maglione
.POSIX:

include config.mk

SRC = area.c bar.c client.c column.c draw.c event.c frame.c fs.c \
	geom.c key.c main.c mouse.c rule.c printevent.c util.c view.c
OBJ = ${SRC:.c=.o}
MAN1 = wmii wmiir wmiiwm wmiiloop
SCRIPTS = wmiistartrc wmiir wmiiloop wmii9rc
BIN = wmii wmii9menu

all: options ${BIN}

options:
	@echo wmii build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

# In case this isn't from hg
.hg/00changelog.i:
	@mkdir .hg
	@touch .hg/00changelog.i
# VERSION must be updated on every commit/pull
config.mk: .hg/00changelog.i

${OBJ}: wmii.h config.mk

9menu.o: config.mk

wmii: ${OBJ}
	@echo LD $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS} ${LIBIXP}

wmii9menu: 9menu.o
	@echo LD $@
	@${CC} -o $@ 9menu.o ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ${BIN} ${OBJ} 9menu.o wmii-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p wmii-${VERSION}
	@ln LICENSE Makefile config.mk README rc/ \
		wmii.eps wmii.mp \
		${MAN1:=.1} ${SRC} ${SCRIPTS} wmii.h 9menu.c \
		wmii-${VERSION}/
	@tar -zcf wmii-${VERSION}.tgz wmii-${VERSION}
	@rm -rf wmii-${VERSION}

install: all
	@echo installing executable files to ${DESTDIR}${PREFIX}/bin
	@mkdir -p -m 0755 ${DESTDIR}${PREFIX}/bin
	@for i in ${SCRIPTS}; do \
		sed "s|CONFPREFIX|${CONFPREFIX}|g; \
		     s|CONFVERSION|${CONFVERSION}|g; \
		     s|P9PATHS|${P9PATHS}|g; \
		     s|AWKPATH|${AWKPATH}|g" \
			$$i \
			>${DESTDIR}${PREFIX}/bin/$$i; \
		chmod 755 ${DESTDIR}${PREFIX}/bin/$$i; \
	 done
	@for i in ${BIN}; do\
		cp -f $$i ${DESTDIR}${PREFIX}/bin; \
		chmod 755 ${DESTDIR}${PREFIX}/bin/$$i; \
	  done
	@echo installing scripts to ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@mkdir -p -m 0755 ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@cd rc; for i in *; do \
		sed "s|CONFPREFIX|${CONFPREFIX}|g" $$i \
			>${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}/$$i; \
		chmod 755 ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}/$$i; \
	 done
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p -m 0755 ${DESTDIR}${MANPREFIX}/man1
	@for i in ${MAN1:=.1}; do \
		sed "s/VERSION/${VERSION}/g; \
		     s|CONFPREFIX|${CONFPREFIX}|g" \
			$$i \
			>${DESTDIR}${MANPREFIX}/man1/$$i; \
		chmod 644 ${DESTDIR}${MANPREFIX}/man1/$$i; \
	 done

uninstall:
	@echo removing executable files from ${DESTDIR}${PREFIX}/bin
	@cd ${DESTDIR}${PREFIX}/bin && rm -f ${SCRIPTS} ${BIN}
	@echo removing scripts from ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@rm -rf ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@cd ${DESTDIR}${MANPREFIX}/man1 && rm -f ${MAN1:=.1}

.PHONY: all options clean dist install uninstall
