##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

noinst_PROGRAMS = \
	repeat \
	sync_flush \
	full_flush \
	memusage \
	crc32 \
	known_sizes \
	hex2bin

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	$(STATIC_CPPFLAGS)

AM_LDFLAGS = $(STATIC_LDFLAGS)

LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
LDADD += $(top_builddir)/lib/libgnu.a
endif

LDADD += $(LTLIBINTL)
