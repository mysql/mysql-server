
AM_CPPFLAGS      = -I$(srcdir) -I$(top_builddir)/include
AM_CPPFLAGS     += -I$(top_builddir)/mytap

AM_CFLAGS        = -Wall -ansi -pedantic

AM_LDFLAGS       = -L$(top_builddir)/mytap -L$(top_builddir)/mysys
AM_LDFLAGS      += -L$(top_builddir)/strings

LDADD            = -lmytap -lmysys -lmystrings

noinst_PROGRAMS  = bitmap.t

bitmap_t_SOURCES = bitmap.t.c
