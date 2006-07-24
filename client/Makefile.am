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

if HAVE_YASSL
  yassl_dummy_link_fix= $(top_srcdir)/extra/yassl/src/dummy.cpp
else
  yassl_dummy_link_fix=
endif
#AUTOMAKE_OPTIONS =              nostdinc
INCLUDES =			-I$(top_builddir)/include \
				-I$(top_srcdir)/include \
				-I$(top_srcdir)/regex \
                                $(openssl_includes)
LIBS =				@CLIENT_LIBS@
LDADD=				@CLIENT_EXTRA_LDFLAGS@ \
                                $(top_builddir)/libmysql/libmysqlclient.la
bin_PROGRAMS =			mysql mysqladmin mysqlcheck mysqlshow \
				mysqldump mysqlimport mysqltest mysqlbinlog \
				mysql_upgrade \
				mysqltestmanagerc mysqltestmanager-pwgen
noinst_HEADERS =		sql_string.h completion_hash.h my_readline.h \
				client_priv.h
mysql_SOURCES =			mysql.cc readline.cc sql_string.cc completion_hash.cc
mysqladmin_SOURCES =		mysqladmin.cc
mysql_LDADD =			@readline_link@ @TERMCAP_LIB@ $(LDADD) $(CXXLDFLAGS)
mysqltest_SOURCES=              mysqltest.c $(top_srcdir)/mysys/my_getsystime.c \
                                $(yassl_dummy_link_fix)
mysqltest_LDADD =		$(top_builddir)/regex/libregex.a $(LDADD)
mysqlbinlog_SOURCES =   mysqlbinlog.cc $(top_srcdir)/mysys/mf_tempdir.c $(top_srcdir)/mysys/my_new.cc
mysqlbinlog_LDADD =		$(LDADD) $(CXXLDFLAGS)
mysqltestmanager_pwgen_SOURCES =   mysqlmanager-pwgen.c 
mysqltestmanagerc_SOURCES=      mysqlmanagerc.c $(yassl_dummy_link_fix)
mysqlcheck_SOURCES=             mysqlcheck.c $(yassl_dummy_link_fix)
mysqlshow_SOURCES=              mysqlshow.c $(yassl_dummy_link_fix)
mysqldump_SOURCES=              mysqldump.c my_user.c \
	                        $(top_srcdir)/mysys/mf_getdate.c \
                                $(yassl_dummy_link_fix)
mysqlimport_SOURCES=            mysqlimport.c $(yassl_dummy_link_fix)
mysql_upgrade_SOURCES=          mysql_upgrade.c $(yassl_dummy_link_fix)
sql_src=log_event.h mysql_priv.h log_event.cc my_decimal.h my_decimal.cc
strings_src=decimal.c

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK \
			-DDEFAULT_MYSQL_HOME="\"$(prefix)\"" \
			-DDATADIR="\"$(localstatedir)\"" 

link_sources:
	for f in $(sql_src) ; do \
	  rm -f $$f; \
	  @LN_CP_F@ $(top_srcdir)/sql/$$f $$f; \
         done; \
        for f in $(strings_src) ; do \
          rm -f $(srcdir)/$$f; \
          @LN_CP_F@ $(top_srcdir)/strings/$$f $$f; \
         done; \
         rm -f $(srcdir)/my_user.c; \
         @LN_CP_F@ $(top_srcdir)/sql-common/my_user.c my_user.c;


# Don't update the files from bitkeeper
%::SCCS/s.%
