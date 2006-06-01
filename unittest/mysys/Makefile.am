
AM_CPPFLAGS      = @ZLIB_INCLUDES@ -I$(top_builddir)/include 
AM_CPPFLAGS     += -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap

AM_LDFLAGS       = -L$(top_builddir)/unittest/mytap -L$(top_builddir)/mysys
AM_LDFLAGS      += -L$(top_builddir)/strings -L$(top_builddir)/dbug

LDADD            = -lmytap -lmysys -ldbug -lmystrings

noinst_PROGRAMS  = bitmap-t base64-t my_atomic-t

