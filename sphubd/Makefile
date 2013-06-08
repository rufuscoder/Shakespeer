BUILT_SOURCES = sphashd_cmd.c sphashd_cmd.h \
	       	ui_cmd.c ui_cmd.h ui_send.c ui_send.h \
		sphashd_send.c sphashd_send.h sphashd_client_cmd.c \
	       	sphashd_client_cmd.h sphashd_client_send.c \
		sphashd_client_send.h \
		notifications.c notifications.h

check_PROGRAMS = user_test tthdb_test extra_slots_test \
		 queue_test queue_directory_test \
		 queue_auto_search_test queue_connect_test \
		 share_test share_search_test \
		 search_listener_test extip_test hub_slots_test

TESTS ?= user_test tthdb_test extra_slots_test \
	queue_test queue_directory_test \
	queue_auto_search_test queue_connect_test \
	share_test share_search_test \
	search_listener_test extip_test hub_slots_test

TOP=..
include ${TOP}/common.mk

CFLAGS += $(HEADLESS_CFLAGS)
LDFLAGS += $(HEADLESS_LDFLAGS)

bin_PROGRAMS=sphubd sphashd
noinst_PROGRAMS=share_tool queue_tool
all-local: ${BUILT_SOURCES} ${bin_PROGRAMS} ${noinst_PROGRAMS}

sphubd_SOURCES=client.c client_cmd.c client_download.c client_upload.c \
	       hub.c hub_cmd.c hub_slots.c hub_list.c \
	       queue_db.c queue.c queue_match.c queue_directory.c \
	       queue_connect.c queue_auto_search.c \
	       search_listener.c \
	       sphubd.c user.c extip.c \
	       ui.c ui_cmd.c ui_send.c ui_list.c globals.c \
	       sphashd_client.c sphashd_client_cmd.c sphashd_client_send.c \
	       share.c share_save.c share_scan.c share_search.c \
	       share_tth.c \
	       share_bloom.c \
	       tthdb.c \
	       notifications.c extra_slots.c

sphashd_SOURCES=sphashd.c sphashd_cmd.c sphashd_send.c

SOURCES=${sphubd_SOURCES} ${sphashd_SOURCES} ${share_tool_SOURCES} ${queue_tool_SOURCES} ${BUILT_SOURCES}

sphubd_OBJS=${sphubd_SOURCES:.c=.o}
sphubd: ${sphubd_OBJS} \
	${TOP}/spclient/libspclient.a \
	${TOP}/splib/libsplib.a
	${LINK}

sphashd_OBJS=${sphashd_SOURCES:.c=.o}
sphashd: ${sphashd_OBJS} \
	${TOP}/splib/libsplib.a
	${LINK}

clean-local:
	rm -f ${BUILT_SOURCES} ${bin_PROGRAMS} ${noinst_PROGRAMS} ${check_PROGRAMS} *.o

share_tool_SOURCES=share_tool.c \
		   share.c share_save.c share_scan.c share_search.c \
		   share_tth.c \
		   share_bloom.c \
		   tthdb.c \
		   sphashd_client.c sphashd_client_cmd.c sphashd_client_send.c \
		   globals.c notifications.c

sphubd.o ui.o extip.o: ${TOP}/version.mk

share_tool_OBJS=${share_tool_SOURCES:.c=.o}
share_tool: ${share_tool_OBJS}
	${LINK}

queue_tool_SOURCES=queue_tool.c
queue_tool_LDADD=queue_db.o globals.o notifications.o queue.o queue_directory.o
queue_tool_OBJS=${queue_tool_SOURCES:.c=.o}
queue_tool: ${queue_tool_OBJS} ${queue_tool_LDADD}
	${LINK}

tthdb_list_SOURCES=tthdb_list.c
tthdb_list_LDADD=tthdb.o globals.o
tthdb_list_OBJS=${tthdb_list_SOURCES:.c=.o}
tthdb_list: ${tthdb_list_OBJS} ${tthdb_list_LDADD}
	${LINK}

tth_upgrade_SOURCES=tth-upgrade.c
tth_upgrade_LDADD=tthdb.o globals.o
tth_upgrade_OBJS=${tth_upgrade_SOURCES:.c=.o}
tth-upgrade: ${tth_upgrade_OBJS} ${tth_upgrade_LDADD}
	${LINK}

tth_convert_SOURCES=tth-convert.c
tth_convert_LDADD=tthdb.o globals.o memtthdb.o
tth_convert_OBJS=${tth_convert_SOURCES:.c=.o}
tth-convert: ${tth_convert_OBJS} ${tth_convert_LDADD}
	${LINK}

notifications.c: notifications.in ${TOP}/support/gen_notification_source.awk 
	awk -f ${TOP}/support/gen_notification_source.awk notifications.in > $@
notifications.h: notifications.in ${TOP}/support/gen_notification_header.awk 
	awk -f ${TOP}/support/gen_notification_header.awk notifications.in > $@

sphashd_client_send.c: ${TOP}/sphubd/sphashd_cmd.in ${TOP}/support/gen_send_source.awk 
	awk -f ${TOP}/support/gen_send_source.awk ${TOP}/sphubd/sphashd_cmd.in > $@
sphashd_client_send.h: ${TOP}/sphubd/sphashd_cmd.in ${TOP}/support/gen_send_header.awk 
	awk -f ${TOP}/support/gen_send_header.awk ${TOP}/sphubd/sphashd_cmd.in > $@

sphashd_client_cmd.c: ${TOP}/sphubd/sphashd_client_cmd.in ${TOP}/support/gen_cmd_source.awk 
	awk -f ${TOP}/support/gen_cmd_source.awk ${TOP}/sphubd/sphashd_client_cmd.in > $@
sphashd_client_cmd.h: ${TOP}/sphubd/sphashd_client_cmd.in ${TOP}/support/gen_cmd_header.awk 
	awk -f ${TOP}/support/gen_cmd_header.awk ${TOP}/sphubd/sphashd_client_cmd.in > $@

sphashd_send.c: ${TOP}/sphubd/sphashd_client_cmd.in ${TOP}/support/gen_send_source.awk 
	awk -f ${TOP}/support/gen_send_source.awk ${TOP}/sphubd/sphashd_client_cmd.in > $@
sphashd_send.h: ${TOP}/sphubd/sphashd_client_cmd.in ${TOP}/support/gen_send_header.awk 
	awk -f ${TOP}/support/gen_send_header.awk ${TOP}/sphubd/sphashd_client_cmd.in > $@

sphashd_cmd.c: ${TOP}/sphubd/sphashd_cmd.in ${TOP}/support/gen_cmd_source.awk 
	awk -f ${TOP}/support/gen_cmd_source.awk ${TOP}/sphubd/sphashd_cmd.in > $@
sphashd_cmd.h: ${TOP}/sphubd/sphashd_cmd.in ${TOP}/support/gen_cmd_header.awk 
	awk -f ${TOP}/support/gen_cmd_header.awk ${TOP}/sphubd/sphashd_cmd.in > $@

ui_cmd.c: ${TOP}/sphubd/ui_cmd.in ${TOP}/support/gen_cmd_source.awk 
	awk -f ${TOP}/support/gen_cmd_source.awk ${TOP}/sphubd/ui_cmd.in > $@
ui_cmd.h: ${TOP}/sphubd/ui_cmd.in ${TOP}/support/gen_cmd_header.awk 
	awk -f ${TOP}/support/gen_cmd_header.awk ${TOP}/sphubd/ui_cmd.in > $@

ui_send.c: ${TOP}/spclient/spclient_cmd.in ${TOP}/support/gen_send_source.awk 
	awk -f ${TOP}/support/gen_send_source.awk ${TOP}/spclient/spclient_cmd.in > $@
ui_send.h: ${TOP}/spclient/spclient_cmd.in ${TOP}/support/gen_send_header.awk 
	awk -f ${TOP}/support/gen_send_header.awk ${TOP}/spclient/spclient_cmd.in > $@

extip_test: extip_test.o ${TOP}/splib/libsplib.a notifications.o
	${LINK}

share_test: share_test.o share_scan.o share_bloom.o tthdb.o \
	globals.o notifications.o
	${LINK}

share_search_test: share_search_test.o \
	share.o share_scan.o share_bloom.o tthdb.o \
	globals.o notifications.o
	${LINK}

search_listener_test: search_listener_test.o \
	search_listener.o hub_list.o user.o notifications.o extip.o
	${LINK}

queue_directory_test: queue_directory_test.o queue_db.o queue.o \
	globals.o notifications.o
	${LINK}

queue_auto_search_test: queue_auto_search_test.o \
	queue_db.o queue.o queue_directory.o globals.o notifications.o
	${LINK}

queue_connect_test: queue_connect_test.o \
	queue_db.o queue.o queue_directory.o globals.o notifications.o
	${LINK}

extra_slots_test: extra_slots_test.o globals.o notifications.o
	${LINK}

user_test: user_test.o user.o
	${LINK}

hub_slots_test: hub_slots_test.o hub_list.o user.o extra_slots.o \
		globals.o notifications.o extip.o
	${LINK}

#share_save_test_SOURCES=share_save_test.c share.c share_save.c globals.c
#share_save_test_LDADD = $(top_builddir)/splib/libsplib.a

queue_test: queue_test.o queue_db.o queue_directory.o globals.o notifications.o
	${LINK}

tthdb_test: tthdb_test.o globals.o
	${LINK}

