# Copyright (C) 2000-2006 MySQL AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
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

PRESCRIPTS =		mysql-test-run.pl

GENSCRIPTS =		mysql-test-run-shell \
			install_test_db \
			mtr \
			mysql-test-run

EXTRA_SCRIPTS = 	mysql-test-run-shell.sh \
			install_test_db.sh \
			valgrind.supp $(PRESCRIPTS)

EXTRA_DIST_DIRS =       t r include lib std_data suite extra

EXTRA_DIST = 		$(EXTRA_DIST_DIRS) $(EXTRA_SCRIPTS) README

test_SCRIPTS = 		$(GENSCRIPTS) $(PRESCRIPTS)

CLEANFILES = 		$(GENSCRIPTS)


# mtr - a shortcut for executing mysql-test-run.pl
mtr:
	@RM@ -f mtr
	@LN_S@ mysql-test-run.pl mtr

# mysql-test-run - a shortcut for executing mysql-test-run.pl
mysql-test-run:
	@RM@ -f mysql-test-run
	@LN_S@ mysql-test-run.pl mysql-test-run

SUFFIXES = .sh

testdir =	        $(prefix)/mysql-test

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
