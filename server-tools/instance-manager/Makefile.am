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

DEFS= -DMYSQL_INSTANCE_MANAGER

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
	-DDEFAULT_PORT="3406" \
	-DPROTOCOL_VERSION=@PROTOCOL_VERSION@

liboptions_a_SOURCES= options.h options.cc priv.h priv.cc

# MySQL sometimes uses symlinks to reuse code 
# All symlinked files are grouped in libnet.a

nodist_libnet_a_SOURCES= password.c pack.c sql_state.c net_serv.cc
nodist_libnet_a_CPPFLAGS= $(CPPFLAGS) -DMYSQL_SERVER

CLEANFILES= net_serv.cc password.c pack.c sql_state.c

net_serv.cc: Makefile
	rm -f $(srcdir)/net_serv.cc
	@LN_CP_F@ $(top_srcdir)/sql/net_serv.cc $(srcdir)/net_serv.cc

password.c: Makefile
	rm -f $(srcdir)/password.c
	@LN_CP_F@ $(top_srcdir)/sql/password.c $(srcdir)/password.c

pack.c: Makefile
	rm -f $(srcdir)/pack.c
	@LN_CP_F@ $(top_srcdir)/sql-common/pack.c $(srcdir)/pack.c

sql_state.c: Makefile
	rm -f $(srcdir)/sql_state.c
	@LN_CP_F@ $(top_srcdir)/sql/sql_state.c $(srcdir)/sql_state.c

bin_PROGRAMS= mysqlmanager

mysqlmanager_SOURCES=	mysqlmanager.cc manager.h manager.cc log.h log.cc \
			thread_registry.h thread_registry.cc \
			listener.h listener.cc \
			mysql_connection.h mysql_connection.cc \
			protocol.h protocol.cc \
			user_map.h user_map.cc \
			messages.h messages.cc \
			$(top_srcdir)/sql/sql_string.cc \
			command.h command.cc \
			commands.h commands.cc \
			factory.h factory.cc \
			instance.h instance.cc \
			instance_map.h instance_map.cc\
			instance_options.h instance_options.cc \
			buffer.h buffer.cc parse.cc parse.h \
			guardian.cc guardian.h common_structures.h \
                        mysql_manager_error.h

mysqlmanager_LDADD=	liboptions.a \
			libnet.a \
			$(top_builddir)/vio/libvio.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/strings/libmystrings.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/libmysql/libmysqlclient.la


tags:
	ctags -R *.h *.cc

# Don't update the files from bitkeeper
%::SCCS/s.%
