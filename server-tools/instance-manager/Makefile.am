# Copyright (C) 2004 MySQL AB
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

INCLUDES= -I$(top_srcdir)/include

DEFS= -DMYSQL_INSTANCE_MANAGER -DMYSQL_SERVER

# As all autoconf variables depend from ${prefix} and being resolved only when
# make is run, we can not put these defines to a header file (e.g. to
# default_options.h, generated from default_options.h.in)
# See automake/autoconf docs for details

noinst_LIBRARIES= liboptions.a libnet.a

liboptions_a_CPPFLAGS= $(CPPFLAGS) \
	-DDEFAULT_PID_FILE_NAME="$(localstatedir)/mysqlmanager.pid" \
	-DDEFAULT_LOG_FILE_NAME="$(localstatedir)/mysqlmanager.log" \
	-DDEFAULT_SOCKET_FILE_NAME="$(localstatedir)/mysqlmanager.sock" \
	-DDEFAULT_PASSWORD_FILE_NAME="$(sysconfdir)/mysqlmanager.passwd" \
	-DDEFAULT_MYSQLD_PATH="$(bindir)/mysqld$(EXEEXT)" \
	-DDEFAULT_USER="root" \
	-DDEFAULT_PASSWORD="" \
	-DDEFAULT_MONITORING_INTERVAL="5" \
	-DDEFAULT_PORT="33006" \
	-DPROTOCOL_VERSION=@PROTOCOL_VERSION@

liboptions_a_SOURCES= options.h options.cc priv.h priv.cc

# MySQL sometimes uses symlinks to reuse code 
# All symlinked files are grouped in libnet.a

nodist_libnet_a_SOURCES= net_serv.cc client_settings.h client.c errmsg.c
libnet_a_LIBADD= $(top_builddir)/sql/password.$(OBJEXT) \
	$(top_builddir)/sql/pack.$(OBJEXT) \
	$(top_builddir)/sql/sql_state.$(OBJEXT)

CLEANFILES= net_serv.cc client.c client_settings.h errmsg.c

net_serv.cc: Makefile
	rm -f $(srcdir)/net_serv.cc
	@LN_CP_F@ $(top_srcdir)/sql/net_serv.cc $(srcdir)/net_serv.cc

client.c: Makefile
	rm -f $(srcdir)/client.c
	@LN_CP_F@ $(top_srcdir)/sql-common/client.c $(srcdir)/client.c

errmsg.c: Makefile
	rm -f $(srcdir)/errmsg.c
	@LN_CP_F@ $(top_srcdir)/libmysql/errmsg.c $(srcdir)/errmsg.c

client_settings.h: Makefile
	rm -f $(srcdir)/client_settings.h
	@LN_CP_F@ $(top_srcdir)/sql/client_settings.h $(srcdir)/client_settings.h

bin_PROGRAMS= mysqlmanager

mysqlmanager_SOURCES=	command.cc command.h mysqlmanager.cc \
			manager.h manager.cc log.h log.cc \
			thread_registry.h thread_registry.cc \
			listener.h listener.cc protocol.h protocol.cc \
			mysql_connection.h mysql_connection.cc \
			user_map.h user_map.cc \
			messages.h messages.cc \
			$(top_srcdir)/sql/sql_string.cc \
			commands.h commands.cc \
			factory.h factory.cc \
			instance.h instance.cc \
			instance_map.h instance_map.cc\
			instance_options.h instance_options.cc \
			buffer.h buffer.cc parse.cc parse.h \
			guardian.cc guardian.h \
                        mysql_manager_error.h client_func.c

mysqlmanager_LDADD=	liboptions.a \
			libnet.a \
			$(top_builddir)/vio/libvio.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/strings/libmystrings.a \
			$(top_builddir)/dbug/libdbug.a \
			@openssl_libs@ @ZLIB_LIBS@


tags:
	ctags -R *.h *.cc

# Don't update the files from bitkeeper
%::SCCS/s.%
