INCLUDES= -I$(top_srcdir)/include

# As all autoconf variables depend from ${prefix} and being resolved only when
# make is run, we can't put these defines to a header file (e.g. to
# default_options.h, generated from default_options.h.in)
# See automake/autoconf docs for details

noinst_LIBRARIES= liboptions.a
liboptions_a_CPPFLAGS= $(CPPFLAGS) \
	-DDEFAULT_PID_FILE_NAME="$(localstatedir)/mysqlmanager.pid" \
	-DDEFAULT_LOG_FILE_NAME="$(localstatedir)/mysqlmanager.log" \
	-DDEFAULT_SOCKET_FILE_NAME="$(localstatedir)/mysqlmanager.sock"

liboptions_a_SOURCES= options.h options.cc

bin_PROGRAMS = mysqlmanager

mysqlmanager_SOURCES= mysqlmanager.cc manager.h manager.cc log.h log.cc \
		      listener.h listener.cc \
		      thread_repository.h thread_repository.cc

mysqlmanager_LDADD= liboptions.a \
		    $(top_builddir)/mysys/libmysys.a \
		    $(top_builddir)/strings/libmystrings.a \
		    $(top_builddir)/dbug/libdbug.a

tags: 
	ctags -R *.h *.cc

# Don't update the files from bitkeeper
%::SCCS/s.%
