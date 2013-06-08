BUILT_SOURCES=spclient_cmd.c spclient_cmd.h \
	      spclient_send.c spclient_send.h country_map.c

TESTS=filelist_xml_test filelist_dclst_test \
      hublist_test ui_connect_test
check_PROGRAMS=$(TESTS)

TOP=..
include ${TOP}/common.mk

CFLAGS += $(HEADLESS_CFLAGS)
LDFLAGS += $(HEADLESS_LDFLAGS)

all-local: libspclient.a

noinst_LIBRARIES = libspclient.a

SOURCES = hublist.c spclient.c \
	 spclient_cmd.c spclient_send.c \
	 country_map.c \
	 filelist.c filelist_xml.c filelist_dclst.c

libspclient.a: ${OBJS}
	rm -f $@
	ar cru $@ ${OBJS}
	ranlib $@

country_map.c: gen_country_map.sh
	/bin/sh gen_country_map.sh > country_map.c

spclient.o: spclient_cmd.c spclient_cmd.h spclient_send.c spclient_send.h

spclient_cmd.c: spclient_cmd.in ${TOP}/support/gen_cmd_source.awk 
	awk -f ${TOP}/support/gen_cmd_source.awk spclient_cmd.in > $@
spclient_cmd.h: spclient_cmd.in ${TOP}/support/gen_cmd_header.awk 
	awk -f ${TOP}/support/gen_cmd_header.awk spclient_cmd.in > $@

spclient_send.c: ${TOP}/sphubd/ui_cmd.in ${TOP}/support/gen_send_source.awk 
	awk -f ${TOP}/support/gen_send_source.awk ${TOP}/sphubd/ui_cmd.in > $@
spclient_send.h: ${TOP}/sphubd/ui_cmd.in ${TOP}/support/gen_send_header.awk 
	awk -f ${TOP}/support/gen_send_header.awk ${TOP}/sphubd/ui_cmd.in > $@

# dummy rule needed for 'make dist' in certain cases
list-en1-semic.txt: country_map.c

filelist_xml_test: filelist_xml_test.o ${TOP}/splib/libsplib.a
	${LINK}

filelist_dclst_test: filelist_dclst_test.o ${TOP}/splib/libsplib.a
	${LINK}

hublist_test: hublist_test.o ${TOP}/splib/libsplib.a
	${LINK}

ui_connect_test: ui_connect_test.o ${TOP}/splib/libsplib.a
	${LINK}

CLEANFILES=*~ PublicHubList.config PublicHubList.xml

clean-local:
	rm -f libspclient.a ${OBJS} ${BUILT_SOURCES}
	rm -f ${CLEANFILES}
	rm -f ${check_PROGRAMS}
	rm -f *.o

