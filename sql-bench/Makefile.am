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

benchdir_root=		$(prefix)
benchdir =		$(benchdir_root)/sql-bench
bench_SCRIPTS =		test-ATIS test-connect test-create test-insert \
			test-big-tables test-select test-wisconsin \
			test-alter-table graph-compare-results \
			bench-init.pl compare-results run-all-tests \
			server-cfg crash-me copy-db bench-count-distinct
CLEANFILES =		$(bench_SCRIPTS)
EXTRA_SCRIPTS =		test-ATIS.sh test-connect.sh test-create.sh \
			test-insert.sh test-big-tables.sh test-select.sh \
			test-alter-table.sh test-wisconsin.sh \
			bench-init.pl.sh compare-results.sh server-cfg.sh \
			run-all-tests.sh crash-me.sh copy-db.sh \
			bench-count-distinct.sh graph-compare-results.sh
EXTRA_DIST =		$(EXTRA_SCRIPTS)

dist-hook:
	mkdir -p $(distdir)/Data/ATIS $(distdir)/Data/Wisconsin \
		 $(distdir)/Results  $(distdir)/Results-win32 \
		 $(distdir)/limits $(distdir)/Comments
	$(INSTALL_DATA) $(srcdir)/Data/ATIS/*.* $(distdir)/Data/ATIS
	$(INSTALL_DATA) $(srcdir)/Data/Wisconsin/*.* $(distdir)/Data/Wisconsin
	$(INSTALL_DATA) $(srcdir)/Results/*-* $(distdir)/Results
	$(INSTALL_DATA) $(srcdir)/Results-win32/*-* $(distdir)/Results-win32
	$(INSTALL_DATA) $(srcdir)/limits/*.* $(distdir)/limits
	$(INSTALL_DATA) $(srcdir)/Comments/*.* $(distdir)/Comments

install-data-local:
	$(mkinstalldirs) \
		 $(DESTDIR)$(benchdir)/Data \
		 $(DESTDIR)$(benchdir)/Data/ATIS \
		 $(DESTDIR)$(benchdir)/Data/Wisconsin \
		 $(DESTDIR)$(benchdir)/Results \
		 $(DESTDIR)$(benchdir)/Results-win32 \
		 $(DESTDIR)$(benchdir)/limits \
		 $(DESTDIR)$(benchdir)/Comments
	$(INSTALL_DATA) $(srcdir)/README  $(DESTDIR)$(benchdir)
	$(INSTALL_DATA) $(srcdir)/Data/ATIS/*.* $(DESTDIR)$(benchdir)/Data/ATIS
	$(INSTALL_DATA) $(srcdir)/Data/Wisconsin/*.* $(DESTDIR)$(benchdir)/Data/Wisconsin
	$(INSTALL_DATA) $(srcdir)/Results/*-* $(DESTDIR)$(benchdir)/Results
	$(INSTALL_DATA) $(srcdir)/Results-win32/*-* $(DESTDIR)$(benchdir)/Results-win32
	$(INSTALL_DATA) $(srcdir)/limits/*.* $(DESTDIR)$(benchdir)/limits
	$(INSTALL_DATA) $(srcdir)/Comments/*.* $(DESTDIR)$(benchdir)/Comments

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''benchdir''@!$(benchdir)!g' \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
