# Copyright (C) 2003-2006 MySQL AB
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

EXTRA_DIST =	Info.plist.sh \
		Description.plist.sh \
		StartupParameters.plist.sh \
		postflight.sh \
		preflight.sh \
		ReadMe.txt \
		MySQLCOM \
		StartupItem.Description.plist \
		StartupItem.Info.plist \
		StartupItem.postinstall

noinst_DATA = Info.plist \
		Description.plist \
		StartupParameters.plist \
		postflight \
		preflight

CLEANFILES =	Info.plist \
		Description.plist \
		StartupParameters.plist \
		postflight \
		preflight

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
		-e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_NUMERIC_VERSION''@!@MYSQL_NUMERIC_VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
		-e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	$< > $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
