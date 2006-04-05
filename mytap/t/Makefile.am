
AM_CPPFLAGS     = -I$(srcdir) -I$(top_builddir)/include -I$(top_builddir)/mytap

AM_LDFLAGS      = -L$(srcdir)/..

AM_CFLAGS       = -Wall -ansi -pedantic

LDADD           = -lmytap

noinst_PROGRAMS = basic.t

basic_t_SOURCES = basic.t.c

all: $(noinst_PROGRAMS)
