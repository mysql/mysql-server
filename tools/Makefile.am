INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include \
			$(openssl_includes) -I../include
LDADD=			@CLIENT_EXTRA_LDFLAGS@ ../libmysql_r/libmysqlclient_r.la @openssl_libs@
bin_PROGRAMS=		mysqlmanager
mysqlmanager_SOURCES=	mysqlmanager.c
mysqlmanager_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
DEF=			-DUNDEF_THREADS_HACK

# Don't update the files from bitkeeper
%::SCCS/s.%
