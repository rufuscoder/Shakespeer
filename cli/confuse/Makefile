TOP=../..
include ${TOP}/common.mk

all-local: libconfuse.a

CFLAGS+=-DPACKAGE_VERSION="\"2.5\"" -DPACKAGE_STRING="\"libConfuse_2.5\""
CFLAGS+=-DHAVE_STRING_H -DHAVE_STDLIB_H -DHAVE_UNISTD_H -DHAVE_STRDUP -DHAVE_STRCASECMP

SOURCES=confuse.c lexer.c

libconfuse.a: ${OBJS}
	rm -f $@
	ar cru $@ $^
	ranlib $@

lexer.c: lexer.l
	lex -Pcfg_yy -o$@ $<

clean-local:
	rm -f ${OBJS} libconfuse.a lexer.c *~

