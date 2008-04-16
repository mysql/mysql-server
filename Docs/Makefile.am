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

EXTRA_DIST =		mysql.info INSTALL-BINARY @extra_docs@

# make sure that "make install" installs the info page, too
# automake only seems to take care of this automatically,
# if we're building the info page from texi directly.
install-data-hook:	$(srcdir)/mysql.info
	if test `basename $(prefix)` = "mysql" ; then \
	  $(mkinstalldirs) $(DESTDIR)$(prefix)/docs ; \
	  $(INSTALL_DATA) $(srcdir)/mysql.info $(DESTDIR)$(prefix)/docs ; \
	  test ! -f $(top_srcdir)/ChangeLog || $(INSTALL_DATA) $(top_srcdir)/ChangeLog $(DESTDIR)$(prefix)/docs ; \
	else \
	  $(mkinstalldirs) $(DESTDIR)$(infodir) $(DESTDIR)$(pkgdatadir) ; \
	  $(INSTALL_DATA) $(srcdir)/mysql.info $(DESTDIR)$(infodir) ; \
	  test ! -f $(top_srcdir)/ChangeLog || $(INSTALL_DATA) $(top_srcdir)/ChangeLog $(DESTDIR)$(pkgdatadir) ; \
	fi

uninstall-local:
	if test `basename $(prefix)` = "mysql" ; then \
	  @RM@ -f $(DESTDIR)$(prefix)/docs/mysql.info ; \
	  @RM@ -f $(DESTDIR)$(prefix)/docs/ChangeLog ; \
	else \
	  @RM@ -f $(DESTDIR)$(infodir)/mysql.info ; \
	  @RM@ -f $(DESTDIR)$(pkgdatadir)/ChangeLog ; \
	fi

# Don't update the files from bitkeeper
%::SCCS/s.%
