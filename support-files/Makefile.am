# Copyright (C) 2000-2001, 2003-2006 MySQL AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

## Process this file with automake to create Makefile.in

EXTRA_DIST =		mysql.spec.sh \
			my-small.cnf.sh \
			my-medium.cnf.sh \
			my-large.cnf.sh \
			my-huge.cnf.sh \
			my-innodb-heavy-4G.cnf.sh \
			mysql-log-rotate.sh \
			mysql.server.sh \
			mysqld_multi.server.sh \
                        config.huge.ini.sh \
                        config.medium.ini.sh \
                        config.small.ini.sh \
			binary-configure.sh \
			magic mysql.m4 \
			MySQL-shared-compat.spec.sh \
			ndb-config-2-node.ini.sh \
			compiler_warnings.supp

SUBDIRS =		MacOSX RHEL4-SElinux

# Default same as 'pkgdatadir', but we can override it
pkgsuppdir =		$(datadir)/@PACKAGE@

pkgsupp_DATA =		my-small.cnf \
			my-medium.cnf \
			my-large.cnf \
			my-huge.cnf \
                        config.huge.ini \
                        config.medium.ini \
                        config.small.ini \
			my-innodb-heavy-4G.cnf \
			mysql-log-rotate \
			binary-configure \
			ndb-config-2-node.ini

pkgsupp_SCRIPTS =	mysql.server \
			mysqld_multi.server

aclocaldir =		$(datadir)/aclocal
aclocal_DATA =		mysql.m4

noinst_DATA = mysql-@VERSION@.spec \
			MySQL-shared-compat.spec

CLEANFILES =		my-small.cnf \
			my-medium.cnf \
			my-large.cnf \
			my-huge.cnf \
                        config.huge.ini \
                        config.medium.ini \
                        config.small.ini \
			my-innodb-heavy-4G.cnf \
			mysql.spec \
			mysql-@VERSION@.spec \
			mysql-log-rotate \
			mysql.server \
			mysqld_multi.server \
			binary-configure \
			MySQL-shared-compat.spec \
			ndb-config-2-node.ini

mysql-@VERSION@.spec:	mysql.spec
			rm -f $@
			cp mysql.spec $@

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''sbindir''@!$(sbindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@SAVE_LDFLAGS@!'\
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''COMPILATION_COMMENT''@!@COMPILATION_COMMENT@!' \
	  -e 's!@''MACHINE_TYPE''@!@MACHINE_TYPE@!' \
	  -e 's!@''HOSTNAME''@!@HOSTNAME@!' \
	  -e 's!@''SYSTEM_TYPE''@!@SYSTEM_TYPE@!' \
	  -e 's!@''CHECK_PID''@!@CHECK_PID@!' \
	  -e 's!@''FIND_PROC''@!@FIND_PROC@!' \
	  -e 's!@''MYSQLD_DEFAULT_SWITCHES''@!@MYSQLD_DEFAULT_SWITCHES@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''TARGET_LINUX''@!@TARGET_LINUX@!' \
	  -e "s!@""CONF_COMMAND""@!@CONF_COMMAND@!" \
	  -e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''SHORT_MYSQL_INTRO''@!@SHORT_MYSQL_INTRO@!' \
	  -e 's!@''SHARED_LIB_VERSION''@!@SHARED_LIB_VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''PERL_DBI_VERSION''@!@PERL_DBI_VERSION@!' \
	  -e 's!@''PERL_DBD_VERSION''@!@PERL_DBD_VERSION@!' \
	  -e 's!@''PERL_DATA_DUMPER''@!@PERL_DATA_DUMPER@!' \
	$< > $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
