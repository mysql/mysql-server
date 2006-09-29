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
SUBDIRS =		. include @docs_dirs@ @zlib_dir@ @yassl_dir@ \
			@readline_topdir@ sql-common \
			@thread_dirs@ pstack \
			@sql_union_dirs@ scripts @man_dirs@ tests \
			netware @libmysqld_dirs@ \
			@bench_dirs@ support-files @tools_dirs@

DIST_SUBDIRS =		. include @docs_dirs@ zlib \
			@readline_topdir@ sql-common \
			@thread_dirs@ pstack \
			@sql_union_dirs@ scripts @man_dirs@ tests SSL\
			BUILD netware os2 @libmysqld_dirs@ \
			@bench_dirs@ support-files @tools_dirs@ win

# Run these targets before any others, also make part of clean target,
# to make sure we create new links after a clean.
BUILT_SOURCES = linked_client_sources linked_server_sources \
		 @linked_client_targets@ \
		 @linked_libmysqld_targets@ \
		 linked_include_sources @linked_netware_sources@

# The db.h file is a bit special, see note in "configure.in".
# In the case we didn't compile with bdb, a dummy file is put
# there, but will not be removed by the bdb make file becuase
# it will never be called.
CLEANFILES = $(BUILT_SOURCES) bdb/build_unix/db.h
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
dist-hook:
	rm -rf `find $(distdir) -type d -name SCCS -print`

tags:
	support-files/build-tags
.PHONY:		init-db bin-dist

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "mysql-test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test:
	cd mysql-test ; \
	./mysql-test-run && \
	./mysql-test-run --ps-protocol

test-force:
	cd mysql-test; \
	./mysql-test-run --force && \
	./mysql-test-run --ps-protocol --force

# We are testing a new Perl version of the test script
test-pl:
	cd mysql-test; \
	./mysql-test-run.pl && \
	./mysql-test-run.pl --ps-protocol

test-force-pl:
	cd mysql-test; \
	./mysql-test-run.pl --force && \
	./mysql-test-run.pl --ps-protocol --force

# Don't update the files from bitkeeper
%::SCCS/s.%

