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

benchdir =		$(prefix)/sql-bench
bench_SCRIPTS =		test-ATIS test-connect test-create test-insert \
			test-big-tables test-select test-wisconsin \
			test-alter-table \
			bench-init.pl compare-results run-all-tests \
			server-cfg crash-me copy-db
CLEANFILES =		$(bench_SCRIPTS)
EXTRA_SCRIPTS =		test-ATIS.sh test-connect.sh test-create.sh \
			test-insert.sh test-big-tables.sh test-select.sh \
			test-alter-table.sh test-wisconsin.sh \
			bench-init.pl.sh compare-results.sh server-cfg.sh \
			run-all-tests.sh crash-me.sh copy-db.sh
EXTRA_DIST =		$(EXTRA_SCRIPTS)

dist-hook:
	mkdir -p $(distdir)/Data/ATIS $(distdir)/Data/Wisconsin \
		 $(distdir)/Results $(distdir)/Results-linux \
		 $(distdir)/Results-win32 \
		 $(distdir)/limits
	cp -pr $(srcdir)/Data/ATIS $(distdir)/Data
	cp -pr $(srcdir)/Data/Wisconsin $(distdir)/Data
	cp -pr $(srcdir)/Results $(distdir)/
	cp -pr $(srcdir)/Results-linux $(distdir)/
	cp -pr $(srcdir)/Results-win32 $(distdir)/
	cp -pr $(srcdir)/limits $(distdir)/

install-data-local:
	$(mkinstalldirs) \
		 $(DESTDIR)$(benchdir)/Data \
		 $(DESTDIR)$(benchdir)/Data/ATIS \
		 $(DESTDIR)$(benchdir)/Data/Wisconsin \
		 $(DESTDIR)$(benchdir)/Results \
		 $(DESTDIR)$(benchdir)/Results-linux \
		 $(DESTDIR)$(benchdir)/Results-win32 \
		 $(DESTDIR)$(benchdir)/limits
	cp -p  $(srcdir)/README  $(DESTDIR)$(benchdir)
	cp -pr $(srcdir)/Data/ATIS  $(DESTDIR)$(benchdir)/Data
	cp -pr $(srcdir)/Data/Wisconsin  $(DESTDIR)$(benchdir)/Data
	cp -pr $(srcdir)/Results  $(DESTDIR)$(benchdir)
	cp -pr $(srcdir)/Results-linux  $(DESTDIR)$(benchdir)
	cp -pr $(srcdir)/Results-win32  $(DESTDIR)$(benchdir)
	cp -pr $(srcdir)/limits  $(DESTDIR)$(benchdir)

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
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
