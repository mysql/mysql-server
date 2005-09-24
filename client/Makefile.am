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

#AUTOMAKE_OPTIONS =              nostdinc
INCLUDES =			-I$(top_builddir)/include -I$(top_srcdir)/include \
				-I$(top_srcdir)/regex $(openssl_includes)
LIBS =				@CLIENT_LIBS@
LDADD=				@CLIENT_EXTRA_LDFLAGS@ \
                                $(top_builddir)/libmysql/libmysqlclient.la
bin_PROGRAMS =			mysql mysqladmin mysqlcheck mysqlshow \
 mysqldump mysqlimport mysqltest mysqlbinlog mysqlmanagerc mysqlmanager-pwgen
noinst_HEADERS =		sql_string.h completion_hash.h my_readline.h \
				client_priv.h
mysql_SOURCES =			mysql.cc readline.cc sql_string.cc completion_hash.cc
mysqladmin_SOURCES =		mysqladmin.cc
mysql_LDADD =			@readline_link@ @TERMCAP_LIB@ $(LDADD) $(CXXLDFLAGS)
mysqlbinlog_LDADD =		$(LDADD) $(CXXLDFLAGS)
mysqltest_SOURCES=		mysqltest.c $(top_srcdir)/mysys/my_getsystime.c
mysqltest_LDADD =		$(top_builddir)/regex/libregex.a $(LDADD)
mysqlbinlog_SOURCES =   mysqlbinlog.cc ../mysys/mf_tempdir.c
mysqlmanagerc_SOURCES =   mysqlmanagerc.c 
sql_src=log_event.h log_event.cc

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK

link_sources:
	for f in $(sql_src) ; do \
	  rm -f $$f; \
	  @LN_CP_F@ $(top_srcdir)/sql/$$f $$f; \
        done;

# Don't update the files from bitkeeper
%::SCCS/s.%
