##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

bin_PROGRAMS = lzmainfo

lzmainfo_SOURCES = lzmainfo.c

lzmainfo_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_builddir)/lib \
	$(DYNAMIC_CPPFLAGS)

lzmainfo_LDFLAGS = $(DYNAMIC_LDFLAGS)
lzmainfo_LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
lzmainfo_LDADD += $(top_builddir)/lib/libgnu.a
endif

lzmainfo_LDADD += $(LTLIBINTL)


dist_man_MANS = lzmainfo.1
