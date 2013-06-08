TESTS = rx_test bloom_test args_test util_test tiger_test tigertree_test \
	base32_test he3_test he3_post_test.sh notification_center_test \
	dstring_test dstring_url_test cmd_table_test quote_test xerr_test \
	xstr_test nfkc_test encoding_test xml_test test_connection_test \
	nmdc_test io_test

check_PROGRAMS = rx_test bloom_test args_test util_test tiger_test \
		 tigertree_test base32_test he3_test \
		 notification_center_test dstring_test dstring_url_test \
		 cmd_table_test quote_test xerr_test xstr_test nfkc_test \
		 encoding_test xml_test test_connection_test nmdc_test io_test

TOP=..
include ${TOP}/common.mk

CFLAGS += $(HEADLESS_CFLAGS) -DICONV_CONST=$(ICONV_CONST)
LDFLAGS += $(HEADLESS_LDFLAGS)

all-local: libsplib.a

noinst_LIBRARIES = libsplib.a

SOURCES = args.c mkpath.c util.c bz2.c \
	  bloom.c tiger.c sboxes.c he3.c base32.c log.c \
	  tigertree.c io.c notification_center.c encoding.c \
	  rx.c test_connection.c dstring.c dstring_url.c \
	  cmd_table.c quote.c nmdc.c base64.c xerr.c xstr.c \
	  nfkc.c iconv_string.c xml.c \
	  uhttp.c

ifeq ($(HAVE_FGETLN),no)
	SOURCES += fgetln.c
endif

BUILT_SOURCES=gunibreak.h gunichartables.h gunicomp.h gunidecomp.h
nfkc.o: ${BUILT_SOURCES}
gunibreak.h gunichartables.h gunicomp.h gunidecomp.h: gen-unicode-tables.pl
	perl -w gen-unicode-tables.pl -both 3.2.0 unicode-spec

libsplib.a: ${OBJS}
	rm -f $@
	ar cru $@ ${OBJS}
	ranlib $@

clean-local:
	rm -f libsplib.a *.o *~
	rm -f ${check_PROGRAMS}

distclean: clean
	rm -f ${BUILT_SOURCES}

EXTRA_DIST=gen-unicode-tables.pl he3_post_test.sh 

test_connection_test: test_connection_test.o nfkc.o dstring.o xstr.o 
	${LINK}

xml_test: xml_test.o nfkc.o dstring.o xstr.o 
	${LINK}

encoding_test: encoding_test.o iconv_string.o dstring.o xstr.o 
	${LINK}

nfkc_test: nfkc_test.o dstring.o xstr.o
	${LINK}

xerr_test: xerr_test.o
	${LINK}

xstr_test: xstr_test.o
	${LINK}

rx_test: rx_test.o xstr.o
	${LINK}

dstring_test: dstring_test.o xstr.o
	${LINK}

dstring_url_test: dstring_url_test.o dstring.o xstr.o uhttp.o
	${LINK}

quote_test: quote_test.o dstring.o xstr.o
	${LINK}

args_test: args_test.o quote.o dstring.o xstr.o
	${LINK}

cmd_table_test: cmd_table_test.o args.o quote.o dstring.o xstr.o
	${LINK}

notification_center_test: notification_center_test.o
	${LINK}

bloom_test: bloom_test.o
	${LINK}

util_test: util_test.o
	${LINK}

tiger_test: tiger_test.o
	${LINK}

tigertree_test: tigertree_test.o
	${LINK}

base32_test: base32_test.o
	${LINK}

nmdc_test: nmdc_test.o
	${LINK}

he3_test: he3_test.o
	${LINK}

io_test: io_test.o xerr.o
	${LINK}

he3_post_test.sh:
	chmod 0755 ${srcdir}/he3_post_test.sh
.PHONY: he3_post_test.sh

