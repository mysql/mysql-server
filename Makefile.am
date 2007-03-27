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

# Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS =	foreign

# These are built from source in the Docs directory
EXTRA_DIST =		INSTALL-SOURCE INSTALL-WIN-SOURCE \
			README COPYING EXCEPTIONS-CLIENT CMakeLists.txt
SUBDIRS =		. include @docs_dirs@ @zlib_dir@ \
			@readline_topdir@ sql-common \
			@thread_dirs@ pstack \
			@sql_union_dirs@ unittest storage plugin \
			@sql_server@ scripts @man_dirs@ tests \
			netware @libmysqld_dirs@ \
			mysql-test support-files sql-bench @tools_dirs@ \
			win

DIST_SUBDIRS =		$(SUBDIRS) BUILD

DISTCLEANFILES = ac_available_languages_fragment

# Create permission databases
init-db:		all
	$(top_builddir)/scripts/mysql_install_db

bin-dist:		all
	$(top_builddir)/scripts/make_binary_distribution @MAKE_BINARY_DISTRIBUTION_OPTIONS@

# Remove BK's "SCCS" subdirectories from source distribution
# Create initial database files for Windows installations.
dist-hook:
	rm -rf `find $(distdir) -type d -name SCCS -print`
	rm -f  `find $(distdir) -type l -print`
	mkdir -p $(distdir)/win
	scripts/mysql_install_db --no-defaults --windows \
		--basedir=$(top_builddir) \
		--datadir=$(distdir)/win/data \
		--srcdir=$(top_srcdir)

tags:
	support-files/build-tags

.PHONY:	init-db bin-dist \
  test    test-force    test-full    test-force-full    test-force-mem \
  test-pl test-force-pl test-full-pl test-force-full-pl test-force-pl-mem \
  test-unit test-ps test-nr test-pr test-ns test-binlog-statement

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "mysql-test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-unit:
	cd unittest && $(MAKE) test

test-ps:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --ps-protocol --mysqld=--binlog-format=mixed

test-nr:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --mysqld=--binlog-format=row

test-pr:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --ps-protocol --mysqld=--binlog-format=row

test-ns:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --mysqld=--binlog-format=mixed

test-binlog-statement:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --mysqld=--binlog-format=statement

test:	test-unit test-ns test-pr

test-full:	test test-nr test-ps

test-force:
	$(MAKE) force=--force test

test-force-full:
	$(MAKE) force=--force test-full

#used by autopush.pl to run memory based tests
test-force-mem:
	$(MAKE) 'force=--force --mem' test

# Keep these for a while
test-pl:	test
test-full-pl:	test-full
test-force-pl:	test-force
test-force-pl-mem:  test-force-mem
test-force-full-pl: test-force-full

# Don't update the files from bitkeeper
%::SCCS/s.%
