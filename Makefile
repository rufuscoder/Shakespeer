default: all

SUBDIRS = splib spclient sphubd cli gui

TOP=.
include common.mk

all-local: config.mk version.h ${EXTERN_DEPENDS}
check-local:
clean-local:

config.mk: configure ${TOP}/support/configure.sub
ifeq ($(BUILD_PROFILE),release)
	@echo "forcing build of all external dependencies in release build"
	echo "ICONV_CONST=const" >config.mk
else
	CFLAGS="$(EXTERN_CFLAGS)" sh ./configure
endif

REPO		?= shakespeer
REPO_URL	?= http://shakespeer.googlecode.com/svn

# default "tag" is the trunk
TAG ?= trunk

RELEASE_DIR=release-build-$(TAG)
ifneq ($(TAG),trunk)
  SVN_PATH=tags/$(TAG)
  DIST_VERSION=$(TAG)
else
  SVN_PATH=trunk
  DIST_VERSION:=snapshot-$(shell date +"%Y%m%d")
endif

current-release:
	$(MAKE) release TAG=$(VERSION)

release-build:
	mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR) && \
	if test -d $(REPO)/.svn; then \
	  cd $(REPO) && \
	  echo "updating sources..." && \
	  svn up; \
	else \
	  echo "getting sources..." && \
	  svn checkout http://shakespeer.googlecode.com/svn/$(SVN_PATH) $(REPO) && \
	  cd $(REPO) ; \
	fi && $(MAKE) all BUILD_PROFILE=release && $(MAKE) check BUILD_PROFILE=release 

release-package:
	@echo Creating disk image...
	cd $(RELEASE_DIR)/$(REPO) && \
		/bin/sh support/mkdmg "$(DIST_VERSION)" . ../.. $(REPO)
	@echo Creating source tarball...
	svn export --force $(RELEASE_DIR)/$(REPO) $(PACKAGE)-$(DIST_VERSION) && \
	tar cvf - $(PACKAGE)-$(DIST_VERSION) | gzip > $(PACKAGE)-$(DIST_VERSION).tar.gz && \
	rm -rf $(PACKAGE)-$(DIST_VERSION)

current-dmg: current-release release-package

release: release-build release-package

dist:
	svn export --force $(PACKAGE)-$(DIST_VERSION) && \
	tar -czf $(PACKAGE)-$(DIST_VERSION).tar.gz $(PACKAGE)-$(DIST_VERSION) && \
	rm -rf $(PACKAGE)-$(DIST_VERSION)

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h

cli:
	$(MAKE) WANT_CLI=yes
.PHONY: cli

help:
	@echo "Availalable make targets:"
	@echo "make - builds the local tree"
	@echo "make release - builds a snapshot release (dmg and source tar.gz) from trunk"
	@echo "make release TAG=X.X.X - builds a release from the specified tag"
	@echo "make current-release - builds a release from the current tag from version.mk"
	@echo "make cli - builds CLI in the local tree"

