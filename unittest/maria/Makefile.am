
AM_CPPFLAGS      = @ZLIB_INCLUDES@ -I$(top_builddir)/include 
AM_CPPFLAGS     += -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap

LDADD 		= $(top_builddir)/unittest/mytap/libmytap.a \
		  $(top_builddir)/storage/maria/libmaria.a \
		  $(top_builddir)/mysys/libmysys.a \
		  $(top_builddir)/dbug/libdbug.a \
		  $(top_builddir)/strings/libmystrings.a

noinst_PROGRAMS  = trxman-t

