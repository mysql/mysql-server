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

SUBDIRS = ndb
DIST_SUBDIRS=ndb

benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test
EXTRA_SCRIPTS = 	mysql-test-run.sh install_test_db.sh valgrind.supp $(PRESCRIPTS)
EXTRA_DIST = 		$(EXTRA_SCRIPTS) 
GENSCRIPTS =		mysql-test-run install_test_db
PRESCRIPTS =		mysql-test-run.pl
test_SCRIPTS = 		$(GENSCRIPTS) $(PRESCRIPTS)
test_DATA =		std_data/client-key.pem std_data/client-cert.pem \
			std_data/cacert.pem std_data/server-cert.pem \
			std_data/server-key.pem 
CLEANFILES = 		$(GENSCRIPTS)

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include -I..


dist-hook:
	mkdir -p \
		$(distdir)/t \
                $(distdir)/extra/binlog_tests \
                $(distdir)/extra/rpl_tests \
		$(distdir)/r \
		$(distdir)/include \
		$(distdir)/std_data \
		$(distdir)/std_data/ndb_backup50 \
		$(distdir)/std_data/ndb_backup51 \
		$(distdir)/lib
	-$(INSTALL_DATA) $(srcdir)/t/*.def $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.test $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.imtest $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.sql $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.disabled $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.opt $(srcdir)/t/*.slave-mi $(distdir)/t
	$(INSTALL_SCRIPT) $(srcdir)/t/*.sh $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.test $(distdir)/extra/binlog_tests
	$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.test $(distdir)/extra/rpl_tests
	-$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.opt $(distdir)/extra/binlog_tests
	-$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.opt $(distdir)/extra/rpl_tests
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/r/*.result $(srcdir)/r/*.require $(distdir)/r
	$(INSTALL_DATA) $(srcdir)/std_data/Moscow_leap $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(srcdir)/std_data/*.000001 $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/des_key_file $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.pem $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.frm $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/ndb_backup50/BACKUP* $(distdir)/std_data/ndb_backup50
	$(INSTALL_DATA) $(srcdir)/std_data/ndb_backup51/BACKUP* $(distdir)/std_data/ndb_backup51
	$(INSTALL_DATA) $(srcdir)/lib/init_db.sql $(distdir)/lib
	$(INSTALL_DATA) $(srcdir)/lib/*.pl $(distdir)/lib

install-data-local:
	$(mkinstalldirs) \
		$(DESTDIR)$(testdir)/t \
		$(DESTDIR)$(testdir)/extra/binlog_tests \
		$(DESTDIR)$(testdir)/extra/rpl_tests \
		$(DESTDIR)$(testdir)/r \
		$(DESTDIR)$(testdir)/include \
		$(DESTDIR)$(testdir)/std_data \
		$(DESTDIR)$(testdir)/std_data/ndb_backup50 \
		$(DESTDIR)$(testdir)/std_data/ndb_backup51 \
		$(DESTDIR)$(testdir)/lib
	$(INSTALL_DATA) $(srcdir)/README $(DESTDIR)$(testdir)
	-$(INSTALL_DATA) $(srcdir)/t/*.def $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.test $(DESTDIR)$(testdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.imtest $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.sql $(DESTDIR)$(testdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.disabled $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.opt $(DESTDIR)$(testdir)/t
	$(INSTALL_SCRIPT) $(srcdir)/t/*.sh $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.slave-mi $(DESTDIR)$(testdir)/t
	$(INSTALL_DATA) $(srcdir)/r/*.result $(DESTDIR)$(testdir)/r
	$(INSTALL_DATA) $(srcdir)/r/*.require $(DESTDIR)$(testdir)/r
	$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.test $(DESTDIR)$(testdir)/extra/binlog_tests
	$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.test $(DESTDIR)$(testdir)/extra/rpl_tests
	-$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.opt $(DESTDIR)$(testdir)/extra/binlog_tests
	-$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.opt $(DESTDIR)$(testdir)/extra/rpl_tests
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(DESTDIR)$(testdir)/include
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.*001 $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/des_key_file $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/Moscow_leap $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.pem $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.frm $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(DESTDIR)$(testdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/ndb_backup50/BACKUP* $(DESTDIR)$(testdir)/std_data/ndb_backup50
	$(INSTALL_DATA) $(srcdir)/std_data/ndb_backup51/BACKUP* $(DESTDIR)$(testdir)/std_data/ndb_backup51
	$(INSTALL_DATA) $(srcdir)/lib/init_db.sql $(DESTDIR)$(testdir)/lib
	$(INSTALL_DATA) $(srcdir)/lib/*.pl $(DESTDIR)$(testdir)/lib

uninstall-local:
	@RM@ -f -r $(DESTDIR)$(testdir)


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
	  -e 's!@''USE_NDBCLUSTER''@!@TEST_NDBCLUSTER@!g' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
