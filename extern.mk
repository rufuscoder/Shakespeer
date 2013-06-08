# 1 = name, 2 = version, 3 = url, 4 = md5
define download
	@echo "downloading $(3)..."
	@cp ../${1}-${2}.tar.gz . || \
	cp ../../${1}-${2}.tar.gz . || \
	cp ../../../${1}-${2}.tar.gz . || \
	curl ${3} -L -o ${1}-${2}.tar.gz || \
	wget ${3} || \
	fetch ${3} || \
	ftp ${3}
	@echo "verifying checksum..."
	@md5=`(md5 ${1}-${2}.tar.gz || \
	    md5sum ${1}-${2}.tar.gz) 2>/dev/null | \
	    sed 's/.*\([0-9a-z]\{32\}\).*/\1/'` && \
	test "$$md5" = "${4}" || \
	{ echo "Wrong MD5 checksum on downloaded file."; false; }
endef

# 1 = name, 2 = version
define unpack
	rm -rf ${1}-install ${1}-build-${2}
	mkdir ${1}-build-${2}
	mkdir ${1}-install
	cd ${1}-build-${2} && \
	{ \
          tar zxf ../${1}-${2}.tar.gz || \
	  (gzcat ../${1}-${2}.tar.gz | tar xf - ); \
	}
endef

EVENT_VER=1.4.8-stable
EVENT_URL=http://monkey.org/~provos/libevent-$(EVENT_VER).tar.gz
EVENT_MD5=e626c34f86746dffe8d055af0a84d08e
libevent-$(EVENT_VER).tar.gz:
	$(call download,libevent,$(EVENT_VER),$(EVENT_URL),$(EVENT_MD5))
libevent: libevent-build-$(EVENT_VER)/stamp
libevent-build-${EVENT_VER}/stamp: libevent-$(EVENT_VER).tar.gz
	$(call unpack,libevent,$(EVENT_VER))
	cwd=`pwd` && cd libevent-build-$(EVENT_VER)/libevent-$(EVENT_VER) && \
	    CFLAGS="$(EXTERN_CFLAGS)" \
	    LDFLAGS="$(EXTERN_CFLAGS)" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/libevent-install && \
	    $(MAKE) && $(MAKE) install
	touch libevent-build-${EVENT_VER}/stamp

EXPAT_VER=2.0.1
EXPAT_URL=http://downloads.sourceforge.net/expat/expat-${EXPAT_VER}.tar.gz
EXPAT_MD5=ee8b492592568805593f81f8cdf2a04c
expat-${EXPAT_VER}.tar.gz:
	$(call download,expat,${EXPAT_VER},${EXPAT_URL},${EXPAT_MD5})
expat: expat-build-${EXPAT_VER}/stamp
expat-build-${EXPAT_VER}/stamp: expat-${EXPAT_VER}.tar.gz
	$(call unpack,expat,${EXPAT_VER})
	cwd=`pwd` && cd expat-build-${EXPAT_VER}/expat-${EXPAT_VER} && \
	    CFLAGS="$(EXTERN_CFLAGS)" \
	    LDFLAGS="$(EXTERN_CFLAGS)" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/expat-install && \
	    ${MAKE} install
	touch expat-build-${EXPAT_VER}/stamp

ICONV_VER=1.12
ICONV_URL=http://ftp.gnu.org/pub/gnu/libiconv/libiconv-$(ICONV_VER).tar.gz
ICONV_MD5=c2be282595751535a618ae0edeb8f648
libiconv-$(ICONV_VER).tar.gz:
	$(call download,libiconv,$(ICONV_VER),$(ICONV_URL),$(ICONV_MD5))
libiconv: libiconv-build-$(ICONV_VER)/stamp
libiconv-build-$(ICONV_VER)/stamp: libiconv-$(ICONV_VER).tar.gz
	$(call unpack,libiconv,$(ICONV_VER))
	cwd=`pwd` && cd libiconv-build-$(ICONV_VER)/libiconv-$(ICONV_VER) && \
	    CFLAGS="$(EXTERN_CFLAGS)" \
	    LDFLAGS="$(EXTERN_CFLAGS)" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/libiconv-install && \
	    $(MAKE) install
	touch libiconv-build-$(ICONV_VER)/stamp

BZ2_VER=1.0.5
BZ2_URL=http://www.bzip.org/$(BZ2_VER)/bzip2-$(BZ2_VER).tar.gz
BZ2_MD5=3c15a0c8d1d3ee1c46a1634d00617b1a
bzip2-$(BZ2_VER).tar.gz:
	$(call download,bzip2,$(BZ2_VER),$(BZ2_URL),$(BZ2_MD5))
bzip2: bzip2-build-$(BZ2_VER)/stamp
bzip2-build-$(BZ2_VER)/stamp: bzip2-$(BZ2_VER).tar.gz
	$(call unpack,bzip2,$(BZ2_VER))
	cwd=`pwd` && cd bzip2-build-$(BZ2_VER)/bzip2-$(BZ2_VER) && \
	    sed 's/^CFLAGS/&+/' Makefile > makefile.tmp && \
	    CFLAGS="$(EXTERN_CFLAGS)" \
	    LDFLAGS="$(EXTERN_CFLAGS)" \
	    $(MAKE) -f makefile.tmp libbz2.a && \
	    mkdir -p $$cwd/bzip2-install/lib && \
	    mkdir -p $$cwd/bzip2-install/include && \
	    cp -f bzlib.h $$cwd/bzip2-install/include && \
	    cp -f libbz2.a $$cwd/bzip2-install/lib/libbz2.a
	touch bzip2-build-$(BZ2_VER)/stamp

ifneq (${HAS_BZ2},yes)
EXTERN_DEPENDS+=bzip2
BZ2_CFLAGS=-I$(TOP)/bzip2-install/include
BZ2_LIBS=$(TOP)/bzip2-install/lib/libbz2.a
endif

ifneq (${HAS_ICONV},yes)
EXTERN_DEPENDS+=libiconv
ICONV_CFLAGS=-I${TOP}/libiconv-install/include
ICONV_LIBS=$(TOP)/libiconv-install/lib/libiconv.a
endif

ifneq (${HAS_EXPAT},yes)
EXTERN_DEPENDS+=expat
EXPAT_CFLAGS=-I$(TOP)/expat-install/include
EXPAT_LIBS=$(TOP)/expat-install/lib/libexpat.a
endif

ifneq ($(HAS_LIBEVENT),yes)
EXTERN_DEPENDS+=libevent
LIBEVENT_CFLAGS=-I$(TOP)/libevent-install/include
LIBEVENT_LIBS=$(TOP)/libevent-install/lib/libevent.a
endif

CFLAGS+=${BZ2_CFLAGS}
CFLAGS+=${ICONV_CFLAGS}
CFLAGS+=${LIBEVENT_CFLAGS}
CFLAGS+=${EXPAT_CFLAGS}

LIBS=-L${TOP}/spclient -lspclient -L${TOP}/splib -lsplib \
     ${ICONV_LDFLAGS} ${ICONV_LIBS} \
     ${BZ2_LDFLAGS} ${BZ2_LIBS} \
     ${EXPAT_LDFLAGS} ${EXPAT_LIBS} \
     ${LIBEVENT_LDFLAGS} ${LIBEVENT_LIBS}
