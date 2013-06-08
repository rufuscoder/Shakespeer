TOP=..
include ${TOP}/common.mk

ifeq (${WANT_CLI},yes)
all-local: .tecla.build confuse/libconfuse.a shakespeer
else
all-local:
endif

CFLAGS += -Ilibtecla/tecla-local-install/include -Iconfuse
LDFLAGS += -Lconfuse
LIBS += -lconfuse libtecla/tecla-local-install/lib/libtecla.a -lcurses

shakespeer_SOURCES = cmd.c ctx-filelist.c ctx-hub.c \
		     ctx-hublist.c ctx-main.c \
		     ctx-queue.c shakespeer.c cfg.c

confuse/libconfuse.a:
	${MAKE} -C confuse libconfuse.a

LIBTECLA_VERSION=1.6.1
LIBTECLA_URL=http://www.astro.caltech.edu/~mcs/tecla/libtecla-${LIBTECLA_VERSION}.tar.gz 
libtecla-${LIBTECLA_VERSION}.tar.gz:
	cp ../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	cp ../../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	cp ../../../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	curl ${LIBTECLA_URL} -o libtecla-${LIBTECLA_VERSION}.tar.gz || \
	wget ${LIBTECLA_URL} || \
	fetch ${LIBTECLA_URL} || \
	ftp ${LIBTECLA_URL}

.tecla.build: tecla-display_message.patch libtecla-${LIBTECLA_VERSION}.tar.gz
	rm -rf libtecla && tar zxvf libtecla-${LIBTECLA_VERSION}.tar.gz && \
	cd libtecla && patch -p1 < ../tecla-display_message.patch && \
	./configure --without-man-pages \
		    --disable-dependency-tracking \
		    --prefix=`pwd`/tecla-local-install && \
	 $(MAKE) install_lib install_inc && cd .. && \
	touch .tecla.build && \
       	ranlib libtecla/tecla-local-install/lib/libtecla.a

shakespeer_OBJS=${shakespeer_SOURCES:.c=.o}
shakespeer: ${shakespeer_OBJS} \
	    ${TOP}/spclient/libspclient.a \
	    ${TOP}/splib/libsplib.a
	${LINK}

clean-local:
	rm -f shakespeer *.o
	-${MAKE} -C libtecla clean

distclean: clean
	rm -rf .tecla.build libtecla


