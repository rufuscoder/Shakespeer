# common makefile for all subdirectories
# must define TOP before including this file, for example:
# TOP = ..
# include $(TOP)/common.mk

default: all

-include $(TOP)/config.mk
include $(TOP)/version.mk
include $(TOP)/extern.mk

# set to yes if you want to build the command line interface
WANT_CLI ?= no

CFLAGS+=-g -O3 -Wall -Werror -Wno-strict-aliasing -DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PACKAGE)\"
CFLAGS+=-I$(TOP)/splib -I${TOP}/spclient

os := $(shell uname)
os_ver := $(shell uname -r | cut -c 1)

ifeq ($(os),Linux)
  # Required for asprintf on Linux
  CFLAGS+=-D_GNU_SOURCE
  # Required for large file support on Linux
  CFLAGS+=-D_FILE_OFFSET_BITS=64
endif

ifeq ($(HAVE_FGETLN),no)
  CFLAGS += -DMISSING_FGETLN
endif

# Disable coredumps for public releases
ifneq ($(BUILD_PROFILE),release)
  CFLAGS+=-DCOREDUMPS_ENABLED=1
endif

# Add -m64 here to compile external libraries with 64-bit support
EXTERN_CFLAGS = $(UB_CFLAGS)
EXTERN_LDFLAGS =

# Add -m64 here to compile non-GUI files with 64-bit support
HEADLESS_CFLAGS =
HEADLESS_LDFLAGS =

# Build a Universal Binary on Mac OS X
ifeq ($(os),Darwin)
  ifeq ($(BUILD_PROFILE),release)
    # Always build releases against 10.4 Universal SDK
    UB_CFLAGS = -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc -mmacosx-version-min=10.4
    UB_LDFLAGS = -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4

    CFLAGS += $(UB_CFLAGS)
    EXTERN_CFLAGS += $(UB_CFLAGS)
    EXTERN_LDFLAGS += $(UB_LDFLAGS)
    HEADLESS_CFLAGS += $(UB_CFLAGS)
    HEADLESS_LDFLAGS += $(UB_LDFLAGS)
  endif
endif

# Need -lresolv on Linux
ifeq ($(os),Linux)
  LIBS+=-lresolv
endif

# search for xcodebuild in path
pathsearch = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))
XCODE := $(call pathsearch,xcodebuild)

# automatic dependency files go into this directory
DEPDIR = .deps
df = $(DEPDIR)/$(*F)

# automatic dependency generation from http://make.paulandlesley.org/autodep.html
# depends on gcc features (the -MD option)

COMPILE = \
	mkdir -p $(DEPDIR); \
	CMD="${CC} -Wp,-MD,$(df).d -c -o $@ $< ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; } && \
	cp $(df).d $(df).P && \
	sed -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(df).d >> $(df).P && \
	rm -f $(df).d

COMPILE.test = \
	mkdir -p $(DEPDIR); \
	CMD="${CC} -Wp,-MD,$(df).d -c -o $@ $< -DTEST ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; } && \
	cp $(df).d $(df).P && \
	sed -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(df).d >> $(df).P && \
	rm -f $(df).d

%.o: %.c
	@echo "compiling $<"
	@$(COMPILE)

%.o: %.m
	@echo "compiling $<"
	@$(COMPILE)

%_test.o: %.c
	@echo "compiling tests in $<"
	@$(COMPILE.test)

define LINK
	@echo "linking $@"
	@CMD="$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)"; $$CMD || \
	  { echo "command was: $$CMD"; false; }
endef

all clean check: 
	@for subdir in $(SUBDIRS); do \
	  echo "making $@ in $$subdir"; \
	  $(MAKE) -C $$subdir $@ || exit 1; \
	done
all: all-local
clean: clean-local clean-deps
celan: clean
check: check-local

clean-deps:
	rm -rf $(DEPDIR)

check-local: $(check_PROGRAMS)
	@for test in $(TESTS); do \
		chmod 0755 ./$$test && \
		echo "=====[ running $$test ]=====" && \
		if ./$$test ; then \
			echo "PASSED: $$test"; \
		else \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

tags:
	ctags $(TOP)/spclient/* $(TOP)/splib/* $(TOP)/sphubd/*

.PHONY: tags

OBJS=$(SOURCES:.c=.o)
ALLCSOURCES := $(wildcard *.c)
ALLMSOURCES := $(wildcard *.m)
ifeq ($(os),Darwin)
  ALLSOURCES := $(ALLCSOURCES) $(ALLMSOURCES)
else
  ALLSOURCES := $(ALLCSOURCES)
endif

ifneq ($(ALLCSOURCES),)
  -include $(patsubst %.c,$(DEPDIR)/%.P,$(ALLCSOURCES))
endif
ifneq ($(ALLMSOURCES),)
  -include $(patsubst %.m,$(DEPDIR)/%.P,$(ALLMSOURCES))
endif
