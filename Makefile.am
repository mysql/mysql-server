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
EXTRA_DIST =		INSTALL-SOURCE README COPYING EXCEPTIONS-CLIENT
SUBDIRS =		. include @docs_dirs@ @zlib_dir@ \
			@readline_topdir@ sql-common \
			@thread_dirs@ pstack @sql_client_dirs@ \
			@sql_server_dirs@ scripts man tests \
			netware @libmysqld_dirs@ \
			@bench_dirs@ support-files @fs_dirs@ @tools_dirs@

DIST_SUBDIRS =		. include @docs_dirs@ zlib \
			@readline_topdir@ sql-common \
			@thread_dirs@ pstack @sql_client_dirs@ \
			@sql_server_dirs@ scripts @man_dirs@ tests SSL\
			BUILD netware os2 @libmysqld_dirs@ \
			@bench_dirs@ support-files @fs_dirs@ @tools_dirs@

# Relink after clean
linked_sources = linked_client_sources linked_server_sources \
		 linked_libmysql_sources linked_libmysql_r_sources \
		 linked_libmysqld_sources  linked_libmysqldex_sources \
		 linked_include_sources @linked_netware_sources@

CLEANFILES = $(linked_sources)

# This is just so that the linking is done early.
all-local: $(linked_sources)

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

#avoid recursive make calls in sql directory
linked_server_sources:
	cd sql; rm -f mini_client_errors.c;\
		@LN_CP_F@ ../libmysql/errmsg.c mini_client_errors.c;\
		rm -f pack.c;@LN_CP_F@ ../sql-common/pack.c pack.c;\
		rm -f client.c;@LN_CP_F@ ../sql-common/client.c client.c;\
		rm -f my_time.c;@LN_CP_F@ ../sql-common/my_time.c my_time.c
	echo timestamp > linked_server_sources

# Create permission databases
init-db:		all
	$(top_builddir)/scripts/mysql_install_db

bin-dist:		all
	$(top_builddir)/scripts/make_binary_distribution @MAKE_BINARY_DISTRIBUTION_OPTIONS@

# Remove BK's "SCCS" subdirectories from source distribution
dist-hook:
	rm -rf `find $(distdir) -type d -name SCCS`

tags:
	support-files/build-tags
.PHONY:		init-db bin-dist

# Test installation

test:
	cd mysql-test ; ./mysql-test-run

