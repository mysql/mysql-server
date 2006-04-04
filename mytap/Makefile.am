
AM_CPPFLAGS  = -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)
AM_CPPFLAGS += -I$(top_builddir)/mytap

noinst_LIBRARIES   = libmytap.a
noinst_HEADERS     = tap.h

libmytap_a_SOURCES = tap.c

SUBDIRS            = . t
