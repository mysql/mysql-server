# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# This file is public domain and comes with NO WARRANTY of any kind

INCLUDES =			-I$(srcdir)/../include $(openssl_includes) \
				-I../include -I$(srcdir)/.. -I$(top_srcdir) \
				-I..
noinst_HEADERS =		client_priv.h
LIBS =				@CLIENT_LIBS@
LDADD =				@CLIENT_EXTRA_LDFLAGS@ ../libmysql/libmysqlclient.la
bin_PROGRAMS =			mysql mysqladmin mysqlcheck mysqlshow \
 mysqldump mysqlimport mysqltest mysqlbinlog
noinst_PROGRAMS =		insert_test select_test thread_test
noinst_HEADERS =		sql_string.h completion_hash.h my_readline.h
mysql_SOURCES =			mysql.cc readline.cc sql_string.cc completion_hash.cc
mysql_LDADD =			@readline_link@ @TERMCAP_LIB@ $(LDADD) $(CXXLDFLAGS)
mysql_DEPENDENCIES=		$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqladmin_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqlcheck_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqlshow_DEPENDENCIES=		$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqldump_DEPENDENCIES=		$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqlimport_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
insert_test_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
select_test_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqltest_SOURCES=			mysqltest.c
mysqltest_DEPENDENCIES=   	$(LIBRARIES) $(pkglib_LTLIBRARIES)
mysqlbinlog_SOURCES =   mysqlbinlog.cc 
mysqlbinlog_DEPENDENCIES=   	$(LIBRARIES) $(pkglib_LTLIBRARIES)
sql_src=log_event.h log_event.cc

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK

link_sources:
	for f in $(sql_src) ; do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(top_srcdir)/sql/$$f $(srcdir)/$$f; \
        done;

thread_test.o:		thread_test.c
			$(COMPILE) -c @MT_INCLUDES@ $(INCLUDES) $<

# Don't update the files from bitkeeper
%::SCCS/s.%
