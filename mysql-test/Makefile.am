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
testdir =	        $(benchdir_root)/mysql-test
EXTRA_SCRIPTS = 	mysql-test-run.sh install_test_db.sh
EXTRA_DIST = 		$(EXTRA_SCRIPTS) 
test_SCRIPTS = 		mysql-test-run install_test_db
CLEANFILES = 		$(test_SCRIPTS)

dist-hook:
	mkdir -p $(distdir)/t $(distdir)/r $(distdir)/include \
		$(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/t/*.test $(srcdir)/t/*.opt $(srcdir)/t/*.sh $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/r/*.result $(srcdir)/r/*.require $(distdir)/r
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(srcdir)/std_data/*.001 $(distdir)/std_data

install-data-local:
	$(mkinstalldirs) \
		$(DESTDIR)$(testdir)/t \
		$(DESTDIR)$(testdir)/r \
		$(DESTDIR)$(testdir)/include \
		$(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/README $(DESTDIR)$(testdir)
	$(INSTALL_DATA) $(srcdir)/t/*.test $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.opt $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/r/*.result $(DESTDIR)$(testdir)/r
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(DESTDIR)$(testdir)/include
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(DESTDIR)$(testdir)/std_data

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''testdir''@!$(testdir)!g' \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
