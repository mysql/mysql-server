INCLUDES =			-I$(srcdir)/../include $(openssl_includes) \
				-I../include -I$(srcdir)/.. -I$(top_srcdir) \
				-I..
LIBS =				@TOOLS_LIBS@
LDADD =				@CLIENT_EXTRA_LDFLAGS@ ../libmysql_r/libmysqlclient_r.la
bin_PROGRAMS =			mysqlmanager
mysqlmanager_SOURCES = mysqlmanager.c 
mysqlmanager_DEPENDENCIES=   	$(LIBRARIES) $(pkglib_LTLIBRARIES)
DEFS =			-DUNDEF_THREADS_HACK

# Don't update the files from bitkeeper
%::SCCS/s.%


