# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
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
			@sql_union_dirs@ storage plugin \
			@sql_server@ scripts @man_dirs@ tests \
			netware @libmysqld_dirs@ \
			mysql-test support-files @tools_dirs@ \
			unittest win

DIST_SUBDIRS =		$(SUBDIRS) BUILD

# Run these targets before any others, also make part of clean target,
# to make sure we create new links after a clean.
BUILT_SOURCES = linked_client_sources linked_server_sources \
		 @linked_client_targets@ \
		 @linked_libmysqld_targets@ \
		 linked_include_sources @linked_netware_sources@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = ac_available_languages_fragment

linked_include_sources:
	cd include; $(MAKE) link_sources
	echo timestamp > linked_include_sources

linked_client_sources:  @linked_client_targets@
	cd client; $(MAKE) link_sources
	echo timestamp > linked_client_sources

linked_libmysql_sources:
	cd libmysql; $(MAKE) link_sources
	echo timestamp > linked_libmysql_sources

linked_libmysql_r_sources: linked_libmysql_sources
	cd libmysql_r; $(MAKE) link_sources
	echo timestamp > linked_libmysql_r_sources

linked_libmysqld_sources:
	cd libmysqld; $(MAKE) link_sources
	echo timestamp > linked_libmysqld_sources

linked_libmysqldex_sources:
	cd libmysqld/examples; $(MAKE) link_sources
	echo timestamp > linked_libmysqldex_sources

linked_netware_sources:
	cd @netware_dir@; $(MAKE) link_sources
	echo timestamp > linked_netware_sources

linked_server_sources:
	cd sql; $(MAKE) link_sources
	echo timestamp > linked_server_sources

# Create permission databases
init-db:		all
	$(top_builddir)/scripts/mysql_install_db

bin-dist:		all
	$(top_builddir)/scripts/make_binary_distribution @MAKE_BINARY_DISTRIBUTION_OPTIONS@

# Remove BK's "SCCS" subdirectories from source distribution
# Create initial database files for Windows installations.
dist-hook:
	rm -rf `find $(distdir) -type d -name SCCS -print`
	if echo "$(distdir)" | grep -q '^/' ; then \
	  scripts/mysql_install_db --no-defaults --windows \
		--basedir=$(top_srcdir) \
		--datadir="$(distdir)/win/data"; \
	else \
	  scripts/mysql_install_db --no-defaults --windows \
		 --basedir=$(top_srcdir) \
		 --datadir="$$(pwd)/$(distdir)/win/data"; \
	fi

tags:
	support-files/build-tags

.PHONY:	init-db bin-dist test test-full test-ps test-nr \
	test-ns test-pr test-unit

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
	./mysql-test-run.pl $(force) --ps-protocol --mysqld=--binlog-format=statement

test-nr:
	cd mysql-test ; \
	./mysql-test-run.pl $(force) --mysqld=--binlog-format=row

test-pr:
	cd mysql-test ; \
	./mysql-test-run.pl $(force) --ps-protocol --mysqld=--binlog-format=row

test-ns:
	cd mysql-test ; \
	./mysql-test-run.pl $(force) --mysqld=--binlog-format=statement

test:	test-unit test-ns test-pr

test-full:	test test-nr test-ps

test-force:
	$(MAKE) force=--force test

test-force-full:
	$(MAKE) force=--force test-full

# Keep these for a while
test-pl:	test
test-full-pl:	test-full
test-force-pl:	test-force
test-force-full-pl: test-force-full

# Don't update the files from bitkeeper
%::SCCS/s.%

