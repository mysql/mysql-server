INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include \
			$(openssl_includes) -I../include
LDADD=			@CLIENT_EXTRA_LDFLAGS@ ../libmysql_r/libmysqlclient_r.la @openssl_libs@
bin_PROGRAMS=		mysqlmanager
mysqlmanager_SOURCES=	mysqlmanager.c
mysqlmanager_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
DEF=			-DUNDEF_THREADS_HACK

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h lex.h \
			wait.h

# Don't update the files from bitkeeper
%::SCCS/s.%
