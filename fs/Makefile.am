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

#called from the top level Makefile

CFLAGS = $(ORBIT_CFLAGS) -g
LFLAGS = $(ORBIT_LIBS)
orbit_idl = @orbit_idl@
orbit_includes = @orbit_includes@
orbit_libs = @orbit_libs@

DISTCLEANFILES = CorbaFS-common.* CorbaFS-stubs.* CorbaFS-skels.* CorbaFS.h

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include \
			-I$(srcdir)/../regex \
			-I$(srcdir) -I../include -I.. -I. \
			-I$(srcdir) -I../include -I.. -I. \
		         $(orbit_includes)
WRAPLIBS=		@WRAPLIBS@
libexec_PROGRAMS =   mysqlcorbafsd
noinst_PROGRAMS =mysqlfs_test 
LDADD = ../libmysql/libmysqlclient.la $(orbit_libs)
mysqlcorbafsd_LDADD =		$(LDADD)  $(CXXLDFLAGS) 
noinst_HEADERS =
mysqlfs_test_SOURCES = mysqlcorbafs_test.c CorbaFS-common.c CorbaFS-stubs.c libmysqlfs.c
mysqlcorbafsd_SOURCES = mysqlcorbafs.c CorbaFS-skels.c database.c CorbaFS-common.c libmysqlfs.c

DEFS =			-DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			@DEFS@
# Don't put lex_hash.h in BUILT_SOURCES as this will give infinite recursion
BUILT_SOURCES = CorbaFS-common.c CorbaFS-stubs.c CorbaFS-skels.c CorbaFS.h
EXTRA_DIST = $(BUILT_SOURCES)
#YFLAGS =		-d

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h lex.h \
			wait.h

link_sources:
	rm -f mini_client_errors.c
	@LN_CP_F@ ../libmysql/errmsg.c mini_client_errors.c


# Don't update the files from bitkeeper
%::SCCS/s.%

idltargets : CorbaFS.idl
	$(ORBIT_IDL) CorbaFS.idl
	$(orbit_idl) CorbaFS.idl

# individual rules

CorbaFS-stubs.c : CorbaFS.idl
	$(orbit_idl) CorbaFS.idl

CorbaFS-common.c : CorbaFS.idl
	$(orbit_idl) CorbaFS.idl

CorbaFS-skels.c : CorbaFS.idl
	$(orbit_idl) CorbaFS.idl

#CorbaFS-client.c : CorbaFS.h

#CorbaFS-server.c : CorbaFS.h

CorbaFS.h : CorbaFS.idl
	$(orbit_idl) CorbaFS.idl
 
