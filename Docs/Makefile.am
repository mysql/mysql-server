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

EXTRA_DIST =		mysql.info INSTALL-BINARY

# make sure that "make install" installs the info page, too
# automake only seems to take care of this automatically,
# if we're building the info page from texi directly.
install-data-hook:	$(srcdir)/mysql.info
	$(mkinstalldirs) $(DESTDIR)$(infodir)
	$(INSTALL_DATA) $(srcdir)/mysql.info $(DESTDIR)$(infodir)

uninstall-local:
	@RM@ -f $(DESTDIR)$(infodir)/mysql.info

# Don't update the files from bitkeeper
%::SCCS/s.%
