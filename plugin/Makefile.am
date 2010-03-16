# Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved. 
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS =	foreign

# extra plugin example files are listed here, to
# keep its Makefile.am cleaner as a template
EXTRA_DIST =	fulltext/configure.in	

SUBDIRS =	@mysql_pg_dirs@
DIST_SUBDIRS =	@mysql_pg_distdirs@

# As of 5.5.3-m3, we want to include the plugin files of a debug build in the package
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir) $(DESTDIR)$(pkglibdir)/plugin
	test ! -d debug || $(TAR) cf - debug | ( cd $(DESTDIR)$(pkglibdir)/plugin && $(TAR) xvf - )

# Don't update the files from bitkeeper
%::SCCS/s.%
