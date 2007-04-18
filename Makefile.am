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
SUBDIRS =		. include @docs_dirs@ @zlib_dir@ @yassl_dir@ \
			@readline_topdir@ sql-common scripts \
			@thread_dirs@ pstack \
			@sql_union_dirs@ @man_dirs@ tests \
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

# Our current filtering of "sql_yacc.cc" in "sql/Makefile.am" creates
# a problem, if a VPATH build and "sql_yacc.cc" was part of the source
# distribution we end up with one "sql_yacc.cc" in the source tree,
# and one in the build tree. This breaks "distcleancheck", until this
# is sorted out we redefine the find that scans for files not removed

distcleancheck_listfiles = find . -name sql_yacc.cc -o -type f -print

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
  test-ps test-ns

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "mysql-test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-ps:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force) --ps-protocol

test-ns:
	cd mysql-test ; \
	@PERL@ ./mysql-test-run.pl $(force)

test:	test-ns test-ps

# To ease script-writing, although in 5.0 it is identical to 'test'
test-full:	test

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
