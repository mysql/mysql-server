# Copyright (C) 2000 MySQL AB
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

EXTRA_DIST= errmsg-utf8.txt

dist-hook:
	for dir in charsets @AVAILABLE_LANGUAGES@; do \
	  test -d $(distdir)/$$dir || mkdir $(distdir)/$$dir; \
	  $(INSTALL_DATA) $(srcdir)/$$dir/*.* $(distdir)/$$dir; \
	done; \
	sleep 1 ; touch $(srcdir)/*/errmsg.sys
	$(INSTALL_DATA) $(srcdir)/charsets/README $(distdir)/charsets
	$(INSTALL_DATA) $(srcdir)/charsets/Index.xml $(distdir)/charsets

all-local:	english/errmsg.sys

# Use the english errmsg.sys as a flag that all errmsg.sys needs to be
# created. Normally these are created by extra/Makefile

english/errmsg.sys: errmsg-utf8.txt
	rm -f $(top_builddir)/include/mysqld_error.h
	(cd $(top_builddir)/extra && $(MAKE))

install-data-local:
	for lang in @AVAILABLE_LANGUAGES@; \
	do \
		$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/$$lang; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.sys \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.sys; \
	done
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/charsets
		$(INSTALL_DATA) $(srcdir)/errmsg-utf8.txt \
	        $(DESTDIR)$(pkgdatadir)/errmsg-utf8.txt; \
	$(INSTALL_DATA) $(srcdir)/charsets/README $(DESTDIR)$(pkgdatadir)/charsets/README
	$(INSTALL_DATA) $(srcdir)/charsets/*.xml $(DESTDIR)$(pkgdatadir)/charsets

# FIXME maybe shouldn't remove, could be needed by other installation?
uninstall-local:
	@RM@ -f -r $(DESTDIR)$(pkgdatadir)

distclean-local:
	@RM@ -f */errmsg.sys

# Do nothing
link_sources:

# Don't update the files from bitkeeper
%::SCCS/s.%
