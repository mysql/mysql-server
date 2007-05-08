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
			@readline_topdir@ sql-common scripts \
			@thread_dirs@ pstack \
			@sql_union_dirs@ unittest storage plugin \
			@sql_server@ @man_dirs@ tests \
			netware @libmysqld_dirs@ \
			mysql-test support-files sql-bench @tools_dirs@ \
			win

DIST_SUBDIRS =		$(SUBDIRS) BUILD debian

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
  test-unit test-ps test-nr test-pr test-ns test-binlog-statement \
  test-ext-funcs test-ext-rpl test-ext-partitions test-ext \
  test-fast test-fast-cursor test-fast-view test-fast-prepare \
  test-full-qa

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
	@PERL@ ./mysql-test-run.pl $(force) $(mem) --ps-protocol --mysqld=--binlog-format=row

test-ns:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) $(mem) --mysqld=--binlog-format=mixed

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
	$(MAKE) force=--force mem=--mem test

test-bt:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --comment=normal --force --timer \
	--skip-ndbcluster --report-features
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl  --comment=ps    --force --timer \
	--skip-ndbcluster --ps-protocol
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --comment=normal+rowrepl --force --timer \
	--skip-ndbcluster --mysqld=--binlog-format=row
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --comment=ps+rowrepl+NDB --force --timer \
	--ps-protocol --mysqld=--binlog-format=row
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --comment=NDB            --force --timer \
	--with-ndbcluster-only
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --force --comment=funcs1_ps --ps-protocol --suite=funcs_1
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --force --comment=funcs2 --suite=funcs_2
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --force --comment=rpl --suite=rpl
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --force --comment=partitions --suite=partitions
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --force --comment=jp --suite=jp

test-bt-debug:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	@PERL@ ./mysql-test-run.pl --comment=debug  --force --timer \
	--skip-ndbcluster --skip-rpl --report-features

# Keep these for a while
test-pl:	test
test-full-pl:	test-full
test-force-pl:	test-force
test-force-pl-mem:  test-force-mem
test-force-full-pl: test-force-full

test-ext-funcs:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl --force --suite=funcs_1 ; \
	@PERL@ ./mysql-test-run.pl --force --suite=funcs_2

test-ext-rpl:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl --force --suite=rpl

test-ext-partitions:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl --force --suite=partitions

test-ext-jp:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl --force --suite=jp

test-ext:	test-ext-funcs test-ext-rpl test-ext-partitions test-ext-jp

test-fast:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(subset) --force --skip-ndb --skip-innodb --skip-im --skip-rpl ; \
	@PERL@ ./mysql-test-run.pl $(subset) --force --suite=funcs_1 --do-test=myisam

test-fast-view:
	$(MAKE) subset=--view-protocol test-fast

test-fast-cursor:
	$(MAKE) subset=--cursor-protocol test-fast

test-fast-prepare:
	$(MAKE) subset=--ps-protocol test-fast

test-full-qa:
	$(MAKE) force=--force test-pr \
	test-binlog-statement test-ext test-fast-view \
	test-fast-cursor test-unit

# Don't update the files from bitkeeper
%::SCCS/s.%
