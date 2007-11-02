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

EXTRA_DIST =		mysql.info INSTALL-BINARY

# make sure that "make install" installs the info page, too
# automake only seems to take care of this automatically,
# if we're building the info page from texi directly.
install-data-hook:	mysql.info
	$(mkinstalldirs) $(DESTDIR)$(infodir)
	$(INSTALL_DATA) $(srcdir)/mysql.info $(DESTDIR)$(infodir)

# Don't update the files from bitkeeper
%::SCCS/s.%
