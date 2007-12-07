# Copyright (C) 2000-2006 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
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

if THREAD_SAFE_CLIENT
LIBMYSQLCLIENT_LA =		$(top_builddir)/libmysql_r/libmysqlclient_r.la
else
LIBMYSQLCLIENT_LA =		$(top_builddir)/libmysql/libmysqlclient.la
endif

INCLUDES =			-I$(top_builddir)/include \
				-I$(top_srcdir)/include \
				-I$(top_srcdir)/regex \
                                $(openssl_includes)

LIBS =				@CLIENT_LIBS@

LDADD=				@CLIENT_EXTRA_LDFLAGS@ $(CLIENT_THREAD_LIBS) \
				$(top_builddir)/libmysql/libmysqlclient.la

noinst_HEADERS =		sql_string.h completion_hash.h my_readline.h \
				client_priv.h

EXTRA_DIST =			get_password.c CMakeLists.txt echo.c

BUILT_SOURCES =			link_sources

CLEANFILES =			$(BUILT_SOURCES)

bin_PROGRAMS =			mysql \
				mysqladmin \
				mysqlbinlog \
				mysqlcheck \
				mysqldump \
				mysqlimport \
				mysqlshow \
				mysqlslap \
				mysqltest \
				mysql_upgrade

mysql_SOURCES =			mysql.cc readline.cc sql_string.cc \
				completion_hash.cc
mysql_LDADD =			@readline_link@ @TERMCAP_LIB@ \
				$(LDADD) $(CXXLDFLAGS)
mysqladmin_SOURCES =		mysqladmin.cc

mysqlbinlog_SOURCES =		mysqlbinlog.cc \
				$(top_srcdir)/mysys/mf_tempdir.c \
				$(top_srcdir)/mysys/my_new.cc \
				$(top_srcdir)/mysys/my_bit.c \
				$(top_srcdir)/mysys/my_bitmap.c \
				$(top_srcdir)/mysys/my_vle.c \
				$(top_srcdir)/mysys/base64.c
mysqlbinlog_LDADD =		$(LDADD) $(CXXLDFLAGS)

mysqldump_SOURCES=              mysqldump.c \
				my_user.c \
	                        $(top_srcdir)/mysys/mf_getdate.c

mysqlimport_SOURCES=		mysqlimport.c

mysqlimport_LDADD =		$(CXXLDFLAGS) $(CLIENT_THREAD_LIBS) \
				@CLIENT_EXTRA_LDFLAGS@ \
				$(LIBMYSQLCLIENT_LA) \
				$(top_builddir)/mysys/libmysys.a

mysqlshow_SOURCES=		mysqlshow.c

mysqlslap_SOURCES=		mysqlslap.c
mysqlslap_CFLAGS=		-DTHREAD -UUNDEF_THREADS_HACK
mysqlslap_LDADD =		$(CXXLDFLAGS) $(CLIENT_THREAD_LIBS) \
				@CLIENT_EXTRA_LDFLAGS@ \
				$(LIBMYSQLCLIENT_LA) \
				$(top_builddir)/mysys/libmysys.a

mysqltest_SOURCES=		mysqltest.c \
				$(top_srcdir)/mysys/my_getsystime.c \
				$(top_srcdir)/mysys/my_copy.c
mysqltest_LDADD =		$(top_builddir)/regex/libregex.a $(LDADD)

mysql_upgrade_SOURCES=          mysql_upgrade.c \
                                $(top_srcdir)/mysys/my_getpagesize.c

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK \
			-DDEFAULT_MYSQL_HOME="\"$(prefix)\"" \
			-DDATADIR="\"$(localstatedir)\""

sql_src=log_event.h mysql_priv.h rpl_constants.h \
	log_event.cc my_decimal.h my_decimal.cc \
	log_event_old.h log_event_old.cc \
	rpl_record_old.h rpl_record_old.cc
strings_src=decimal.c dtoa.c

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
	echo timestamp > link_sources;

# Don't update the files from bitkeeper
%::SCCS/s.%
