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

if HAVE_NDBCLUSTER_DB
SUBDIRS = ndb
DIST_SUBDIRS=ndb
USE_NDBCLUSTER=\"--ndbcluster\"
else
# If one uses automake conditionals, automake will automatically
# include all possible branches to DIST_SUBDIRS goal.
# Reset DIST_SUBDIRS if we don't use NDB
SUBDIRS=
DIST_SUBDIRS=
USE_NDBCLUSTER=\"\"
endif

benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test
EXTRA_SCRIPTS = 	mysql-test-run.sh install_test_db.sh $(PRESCRIPTS)
EXTRA_DIST = 		$(EXTRA_SCRIPTS) 
GENSCRIPTS =		mysql-test-run install_test_db
PRESCRIPTS =		mysql-test-run.pl
test_SCRIPTS = 		$(GENSCRIPTS) $(PRESCRIPTS)
test_DATA = std_data/client-key.pem std_data/client-cert.pem std_data/cacert.pem \
	std_data/server-cert.pem std_data/server-key.pem 
CLEANFILES = 		$(GENSCRIPTS) $(test_DATA)

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include -I..
EXTRA_PROGRAMS =	mysql_test_run_new
noinst_HEADERS =	my_manage.h
mysql_test_run_new_SOURCES=		mysql_test_run_new.c my_manage.c my_create_tables.c


dist-hook:
	mkdir -p $(distdir)/t $(distdir)/r $(distdir)/include \
		$(distdir)/std_data $(distdir)/lib
	-$(INSTALL_DATA) $(srcdir)/t/*.def $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.test $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.disabled $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.opt $(srcdir)/t/*.sh $(srcdir)/t/*.slave-mi $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/r/*.result $(srcdir)/r/*.require $(distdir)/r
	$(INSTALL_DATA) $(srcdir)/std_data/Moscow_leap $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(srcdir)/std_data/*.000001 $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/des_key_file $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.pem $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/lib/init_db.sql $(distdir)/lib
	$(INSTALL_DATA) $(srcdir)/lib/*.pl $(distdir)/lib


install-data-local:
	$(mkinstalldirs) \
		$(DESTDIR)$(testdir)/t \
		$(DESTDIR)$(testdir)/r \
		$(DESTDIR)$(testdir)/include \
		$(DESTDIR)$(testdir)/std_data \
		$(DESTDIR)$(testdir)/lib
	$(INSTALL_DATA) $(srcdir)/README $(DESTDIR)$(testdir)
	-$(INSTALL_DATA) $(srcdir)/t/*.def $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.test $(DESTDIR)$(testdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.disabled $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.opt $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.sh $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.slave-mi $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/r/*.result $(DESTDIR)$(testdir)/r
	$(INSTALL_DATA) $(srcdir)/r/*.require $(DESTDIR)$(testdir)/r
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(DESTDIR)$(testdir)/include
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.*001 $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/des_key_file $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/Moscow_leap $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.pem $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/lib/init_db.sql $(DESTDIR)$(testdir)/lib
	$(INSTALL_DATA) $(srcdir)/lib/*.pl $(DESTDIR)$(testdir)/lib

uninstall-local:
	@RM@ -f -r $(DESTDIR)$(testdir)

std_data/client-key.pem:
	@CP@ $(top_srcdir)/SSL/$(@F) $(srcdir)/std_data
std_data/client-cert.pem:
	@CP@ $(top_srcdir)/SSL/$(@F) $(srcdir)/std_data
std_data/cacert.pem:
	@CP@ $(top_srcdir)/SSL/$(@F) $(srcdir)/std_data
std_data/server-cert.pem:
	@CP@ $(top_srcdir)/SSL/$(@F) $(srcdir)/std_data
std_data/server-key.pem:
	@CP@ $(top_srcdir)/SSL/$(@F) $(srcdir)/std_data

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
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''USE_NDBCLUSTER''@!$(USE_NDBCLUSTER)!g' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
