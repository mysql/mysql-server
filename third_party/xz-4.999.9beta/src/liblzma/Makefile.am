##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

SUBDIRS = api

EXTRA_DIST =
CLEANFILES =
doc_DATA =

lib_LTLIBRARIES = liblzma.la
liblzma_la_SOURCES =
liblzma_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common \
	-I$(top_srcdir)/src/liblzma/check \
	-I$(top_srcdir)/src/liblzma/lz \
	-I$(top_srcdir)/src/liblzma/rangecoder \
	-I$(top_srcdir)/src/liblzma/lzma \
	-I$(top_srcdir)/src/liblzma/subblock \
	-I$(top_srcdir)/src/liblzma/delta \
	-I$(top_srcdir)/src/liblzma/simple \
	-I$(top_srcdir)/src/common
liblzma_la_LDFLAGS = -no-undefined -version-info 0:0:0

include $(srcdir)/common/Makefile.inc
include $(srcdir)/check/Makefile.inc

if COND_FILTER_LZ
include $(srcdir)/lz/Makefile.inc
endif

if COND_FILTER_LZMA1
include $(srcdir)/lzma/Makefile.inc
include $(srcdir)/rangecoder/Makefile.inc
endif

if COND_FILTER_SUBBLOCK
include $(srcdir)/subblock/Makefile.inc
endif

if COND_FILTER_DELTA
include $(srcdir)/delta/Makefile.inc
endif

if COND_FILTER_SIMPLE
include $(srcdir)/simple/Makefile.inc
endif


## Windows-specific stuff

# Windows resource compiler support. libtool knows what to do with .rc
# files, but Automake (<= 1.11 at least) doesn't know.
#
# We want the resource file only in shared liblzma. To avoid linking it into
# static liblzma, we overwrite the static object file with an object file
# compiled from empty input. Note that GNU-specific features are OK here,
# because on Windows we are compiled with the GNU toolchain.
.rc.lo:
	$(LIBTOOL) --mode=compile $(RC) $(DEFS) $(DEFAULT_INCLUDES) \
		$(INCLUDES) $(liblzma_la_CPPFLAGS) $(CPPFLAGS) $(RCFLAGS) \
		-i $< -o $@
	echo > empty.c
	$(COMPILE) -c empty.c -o $(*D)/$(*F).o

# Remove ordinals from the generated .def file. People must link by name,
# not by ordinal, because no one is going to track the ordinal numbers.
liblzma.def: liblzma.la liblzma.def.in
	sed 's/ \+@ *[0-9]\+//' liblzma.def.in > liblzma.def

# Creating liblzma.def.in is a side effect of linking the library.
liblzma.def.in: liblzma.la

if COND_W32
CLEANFILES += liblzma.def liblzma.def.in empty.c
liblzma_la_SOURCES += liblzma_w32res.rc
liblzma_la_LDFLAGS += -Xlinker --output-def -Xlinker liblzma.def.in

## liblzma.def.in is created only when building shared liblzma, so don't
## try to create liblzma.def when not building shared liblzma.
if COND_SHARED
doc_DATA += liblzma.def
endif
endif


## pkg-config
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = liblzma.pc
EXTRA_DIST += liblzma.pc.in
