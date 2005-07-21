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

EXTRA_DIST =	Info.plist.sh \
		Description.plist.sh \
		StartupParameters.plist.sh \
		postinstall.sh \
		preinstall.sh \
		ReadMe.txt \
		MySQL \
		StartupItem.Description.plist \
		StartupItem.Info.plist \
		StartupItem.postinstall

CLEANFILES =	Info.plist \
		Description.plist \
		StartupParameters.plist \
		postinstall \
		preinstall

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
		-e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
		-e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	$< > $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
