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

## Process this file with automake to create Makefile.in

BUILT_SOURCES = mysql_fix_privilege_tables.sql \
		mysql_fix_privilege_tables_sql.c

EXTRA_PROGRAMS = 	comp_sql

bin_SCRIPTS =		@server_scripts@ \
			msql2mysql \
			mysql_config \
			mysql_fix_privilege_tables \
			mysql_fix_extensions \
			mysql_setpermission \
			mysql_secure_installation \
			mysql_zap \
			mysqlaccess \
			mysqlbug \
			mysql_convert_table_format \
			mysql_find_rows \
			mysqlhotcopy \
			mysqldumpslow \
			mysqld_multi

noinst_SCRIPTS =	make_binary_distribution \
			make_sharedlib_distribution

EXTRA_SCRIPTS =		make_binary_distribution.sh \
			make_sharedlib_distribution.sh \
			msql2mysql.sh \
			mysql_config.sh \
			mysql_config.pl.in \
			mysql_fix_privilege_tables.sh \
			mysql_fix_extensions.sh \
			mysql_install_db.sh \
			mysql_install_db.pl.in \
			mysql_setpermission.sh \
			mysql_secure_installation.sh \
			mysql_zap.sh \
			mysqlaccess.sh \
			mysqlbug.sh \
			mysql_convert_table_format.sh \
			mysql_find_rows.sh \
			mysqlhotcopy.sh \
			mysqldumpslow.sh \
			mysqld_multi.sh \
			mysqld_safe.sh

EXTRA_DIST =		$(EXTRA_SCRIPTS) \
			mysqlaccess.conf \
			mysqlbug \
			make_win_bin_dist \
			mysql_fix_privilege_tables_sql.c \
			mysql_system_tables_fix.sql \
			CMakeLists.txt

dist_pkgdata_DATA =	fill_help_tables.sql \
			mysql_fix_privilege_tables.sql \
			mysql_system_tables.sql \
			mysql_system_tables_data.sql \
			mysql_test_data_timezone.sql

CLEANFILES =		@server_scripts@ \
			make_binary_distribution \
			make_sharedlib_distribution \
			msql2mysql \
			mysql_config \
			mysql_fix_privilege_tables \
			mysql_fix_extensions \
			mysql_setpermission \
			mysql_secure_installation \
			mysql_zap \
			mysqlaccess \
			mysql_convert_table_format \
			mysql_find_rows \
			mysqlhotcopy \
			mysqldumpslow \
			mysql_tableinfo \
			mysqld_multi

# Default same as 'pkgdatadir', but we can override it
pkgsuppdir =		$(datadir)/@PACKAGE@

# mysqlbug should be distributed built so that people can report build
# failures with it.
DISTCLEANFILES =        $(BUILT_SOURCES) mysqlbug

# We want the right version and configure comand line in mysqlbug
mysqlbug: ${top_builddir}/config.status mysqlbug.sh

# Build mysql_fix_privilege_tables.sql from the files that contain
# the system tables for this version of MySQL plus any commands
# needed to upgrade the system tables from an older version
mysql_fix_privilege_tables.sql: mysql_system_tables.sql \
				mysql_system_tables_fix.sql
	@echo "Building $@";
	@cat mysql_system_tables.sql mysql_system_tables_fix.sql > $@

#
# Build mysql_fix_privilege_tables_sql.c from
# mysql_fix_privileges_tables.sql using comp_sql
# The "sleep" ensures the generated file has a younger timestamp than its source
# (which may have been generated in this very same "make" run).
#
mysql_fix_privilege_tables_sql.c: comp_sql.c mysql_fix_privilege_tables.sql
	$(MAKE) $(AM_MAKEFLAGS) comp_sql$(EXEEXT)
	sleep 2
	$(top_builddir)/scripts/comp_sql$(EXEEXT) \
	  mysql_fix_privilege_tables \
	    $(top_srcdir)/scripts/mysql_fix_privilege_tables.sql $@


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
	  -e 's!@''pkglibdir''@!$(pkglibdir)!g' \
	  -e 's!@''pkgincludedir''@!$(pkgincludedir)!g' \
	  -e 's!@''pkgdatadir''@!$(pkgdatadir)!g' \
	  -e 's!@''pkgsuppdir''@!$(pkgsuppdir)!g' \
	  -e 's!@''sysconfdir''@!$(sysconfdir)!g' \
	  -e 's!@''mandir''@!$(mandir)!g' \
	  -e 's!@''infodir''@!$(infodir)!g' \
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''SAVE_CC''@!@SAVE_CC@!'\
	  -e 's!@''SAVE_CXX''@!@SAVE_CXX@!'\
	  -e 's!@''CC_VERSION''@!@CC_VERSION@!'\
	  -e 's!@''CXX_VERSION''@!@CXX_VERSION@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''SAVE_ASFLAGS''@!@SAVE_ASFLAGS@!'\
	  -e 's!@''SAVE_CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''SAVE_CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''SAVE_LDFLAGS''@!@SAVE_LDFLAGS@!'\
	  -e 's!@''ASFLAGS''@!@ASFLAGS@!'\
	  -e 's!@''CFLAGS''@!@CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@LDFLAGS@!'\
	  -e 's!@''CLIENT_LIBS''@!@CLIENT_LIBS@!' \
	  -e 's!@''ZLIB_LIBS''@!@ZLIB_LIBS@!' \
	  -e 's!@''LIBS''@!@LIBS@!' \
	  -e 's!@''WRAPLIBS''@!@WRAPLIBS@!' \
	  -e 's!@''innodb_system_libs''@!@innodb_system_libs@!' \
	  -e 's!@''openssl_libs''@!@openssl_libs@!' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''COMPILATION_COMMENT''@!@COMPILATION_COMMENT@!' \
	  -e 's!@''MACHINE_TYPE''@!@MACHINE_TYPE@!' \
	  -e 's!@''HOSTNAME''@!@HOSTNAME@!' \
	  -e 's!@''SYSTEM_TYPE''@!@SYSTEM_TYPE@!' \
	  -e 's!@''CHECK_PID''@!@CHECK_PID@!' \
	  -e 's!@''FIND_PROC''@!@FIND_PROC@!' \
	  -e 's!@''MYSQLD_DEFAULT_SWITCHES''@!@MYSQLD_DEFAULT_SWITCHES@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''MYSQL_TCP_PORT_DEFAULT''@!@MYSQL_TCP_PORT_DEFAULT@!' \
	  -e 's!@''TARGET_LINUX''@!@TARGET_LINUX@!' \
	  -e "s!@""CONF_COMMAND""@!@CONF_COMMAND@!" \
	  -e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	  -e 's!@''STATIC_NSS_FLAGS''@!@STATIC_NSS_FLAGS@!' \
	  -e 's!@''NON_THREADED_LIBS''@!@NON_THREADED_LIBS@!' \
	  -e 's!@''ZLIB_DEPS''@!@ZLIB_DEPS@!' \
	  -e "s!@MAKE@!$(MAKE)!" \
	$< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@


# Don't update the files from bitkeeper
%::SCCS/s.%
