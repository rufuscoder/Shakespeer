TOP=../../..
include ${TOP}/common.mk

all-local: Info.plist

Info.plist: Info.plist.in ${TOP}/version.mk
	sed -e "s/@VERSION@/${VERSION}/" \
	    -e "s/@PACKAGE@/${PACKAGE}/" \
	    -e "s/[$$](EXECUTABLE_NAME)/${PACKAGE}/" \
	$< > $@

clean-local:

