# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
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

man_MANS =	mysql.1 isamchk.1 isamlog.1 mysql_zap.1 mysqlaccess.1 \
		mysqladmin.1 mysqld.1 mysqld_multi.1 mysqldump.1 mysqlshow.1 \
		perror.1 replace.1 mysqld_safe.1 mysql_fix_privilege_tables.1

EXTRA_DIST =	mysql.1.in isamchk.1.in isamlog.1.in mysql_zap.1.in \
		mysqlaccess.1.in mysqladmin.1.in mysqld.1.in mysqld_multi.1.in \
		mysqldump.1.in mysqlshow.1.in perror.1.in replace.1.in mysqlman.1.in \
		mysqld_safe.1.in mysql_fix_privilege_tables.1.in

CLEANFILES = $(man_MANS)

SUFFIXES = .in

.in:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	$< > $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
