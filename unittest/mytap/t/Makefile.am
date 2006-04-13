
AM_CPPFLAGS     = -I$(srcdir) -I$(top_builddir)/include
AM_CPPFLAGS    += -I$(srcdir)/..

AM_LDFLAGS      = -L$(top_builddir)/unittest/mytap

LDADD           = -lmytap

noinst_PROGRAMS = basic.t

basic_t_SOURCES = basic.t.c

all: $(noinst_PROGRAMS)
