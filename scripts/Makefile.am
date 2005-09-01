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

## Process this file with automake to create Makefile.in

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
			mysql_explain_log \
			mysqld_multi \
			mysql_create_system_tables

noinst_SCRIPTS =	make_binary_distribution \
			make_sharedlib_distribution \
			make_win_src_distribution

EXTRA_SCRIPTS =		make_binary_distribution.sh \
			make_sharedlib_distribution.sh \
			make_win_src_distribution.sh \
			msql2mysql.sh \
			mysql_config.sh \
			mysql_fix_privilege_tables.sh \
			mysql_fix_extensions.sh \
			mysql_install_db.sh \
			mysql_setpermission.sh \
			mysql_secure_installation.sh \
			mysql_zap.sh \
			mysqlaccess.sh \
			mysqlbug.sh \
			mysql_convert_table_format.sh \
			mysql_find_rows.sh \
			mysqlhotcopy.sh \
			mysqldumpslow.sh \
			mysql_explain_log.sh \
			mysqld_multi.sh \
			mysqld_safe.sh \
			mysql_create_system_tables.sh

EXTRA_DIST =		$(EXTRA_SCRIPTS) \
			mysqlaccess.conf \
			mysqlbug

dist_pkgdata_DATA =		fill_help_tables.sql mysql_fix_privilege_tables.sql

# mysqlbug should be distributed built so that people can report build
# failures with it.
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
			mysqld_multi \
			make_win_src_distribution \
			make_win_binary_distribution \
			mysql_create_system_tables

DISTCLEANFILES =        mysqlbug

# We want the right version and configure comand line in mysqlbug
mysqlbug: ${top_builddir}/config.status mysqlbug.sh

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
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''CC_VERSION''@!@CC_VERSION@!'\
	  -e 's!@''CXX_VERSION''@!@CXX_VERSION@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''ASFLAGS''@!@SAVE_ASFLAGS@!'\
	  -e 's!@''CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@SAVE_LDFLAGS@!'\
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
