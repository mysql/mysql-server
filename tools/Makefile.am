INCLUDES =			-I$(srcdir)/../include $(openssl_includes) \
				-I../include -I$(srcdir)/.. -I$(top_srcdir) \
				-I..
LIBS =				@TOOLS_LIBS@
LDADD =				@CLIENT_EXTRA_LDFLAGS@ ../libmysql_r/libmysqlclient_r.la
bin_PROGRAMS =			mysqlmngd
mysqlmngd_SOURCES = mysqlmngd.c my_vsnprintf.c 
mysqltest_DEPENDENCIES=   	$(LIBRARIES) $(pkglib_LTLIBRARIES)
DEFS =			-DUNDEF_THREADS_HACK

mysys_src=my_vsnprintf.c mysys_priv.h

link_sources:
	for f in $(mysys_src); do \
	rm -f $$f; \
	@LN_CP_F@ ../mysys/$$f $$f; \
	done;

# Don't update the files from bitkeeper
%::SCCS/s.%


