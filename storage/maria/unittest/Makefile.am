# Copyright (C) 2006-2008 MySQL AB
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

AM_CPPFLAGS      = @ZLIB_INCLUDES@ -I$(top_builddir)/include \
                  -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap
INCLUDES         = @ZLIB_INCLUDES@ -I$(top_builddir)/include \
                  -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap
EXTRA_DIST=	ma_test_all-t CMakeLists.txt \
		ma_test_recovery.pl ma_test_recovery.expected
# Only reason to link with libmyisam.a here is that it's where some fulltext
# pieces are (but soon we'll remove fulltext dependencies from Maria).
LDADD=			$(top_builddir)/unittest/mytap/libmytap.a \
			$(top_builddir)/storage/maria/libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
noinst_PROGRAMS =	ma_control_file-t trnman-t \
			ma_pagecache_single_1k-t ma_pagecache_single_8k-t \
			ma_pagecache_single_64k-t \
			ma_pagecache_consist_1k-t \
			ma_pagecache_consist_64k-t \
			ma_pagecache_consist_1kHC-t \
			ma_pagecache_consist_64kHC-t \
			ma_pagecache_consist_1kRD-t \
			ma_pagecache_consist_64kRD-t \
			ma_pagecache_consist_1kWR-t \
			ma_pagecache_consist_64kWR-t \
                        ma_pagecache_rwconsist_1k-t \
                        ma_pagecache_rwconsist2_1k-t \
			ma_test_loghandler-t \
                        ma_test_loghandler_multigroup-t \
			ma_test_loghandler_multithread-t \
			ma_test_loghandler_multiflush-t \
			ma_test_loghandler_pagecache-t \
			ma_test_loghandler_long-t \
			ma_test_loghandler_noflush-t \
			ma_test_loghandler_first_lsn-t \
                        ma_test_loghandler_max_lsn-t \
                        ma_test_loghandler_purge-t \
			ma_test_loghandler_readonly-t\
                        ma_test_loghandler_nologs-t

ma_test_loghandler_t_SOURCES = ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_multigroup_t_SOURCES = ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c sequence_storage.c sequence_storage.h
ma_test_loghandler_multithread_t_SOURCES = ma_test_loghandler_multithread-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_multiflush_t_SOURCES = ma_test_loghandler_multithread-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_multiflush_t_CPPFLAGS = -DMULTIFLUSH_TEST
ma_test_loghandler_pagecache_t_SOURCES = ma_test_loghandler_pagecache-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_long_t_SOURCES = ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_long_t_CPPFLAGS = -DLONG_LOG_TEST
ma_test_loghandler_noflush_t_SOURCES = ma_test_loghandler_noflush-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_first_lsn_t_SOURCES = ma_test_loghandler_first_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_max_lsn_t_SOURCES = ma_test_loghandler_max_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_purge_t_SOURCES = ma_test_loghandler_purge-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c
ma_test_loghandler_readonly_t_SOURCES = ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c sequence_storage.c sequence_storage.h
ma_test_loghandler_readonly_t_CPPFLAGS = -DREADONLY_TEST
ma_test_loghandler_nologs_t_SOURCES = ma_test_loghandler_nologs-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c

ma_pagecache_single_src =	ma_pagecache_single.c test_file.c test_file.h
ma_pagecache_consist_src =	ma_pagecache_consist.c test_file.c test_file.h
ma_pagecache_common_cppflags =	-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN

ma_pagecache_single_1k_t_SOURCES =	$(ma_pagecache_single_src)
ma_pagecache_single_8k_t_SOURCES =	$(ma_pagecache_single_src)
ma_pagecache_single_64k_t_SOURCES =	$(ma_pagecache_single_src)
ma_pagecache_single_1k_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=1024
ma_pagecache_single_8k_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=8192
ma_pagecache_single_64k_t_CPPFLAGS =	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=65536 -DBIG

ma_pagecache_consist_1k_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_1k_t_CPPFLAGS =	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=1024
ma_pagecache_consist_64k_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_64k_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=65536

ma_pagecache_consist_1kHC_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_1kHC_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY
ma_pagecache_consist_64kHC_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_64kHC_t_CPPFLAGS =	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY

ma_pagecache_consist_1kRD_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_1kRD_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=1024 -DTEST_READERS
ma_pagecache_consist_64kRD_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_64kRD_t_CPPFLAGS =	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=65536 -DTEST_READERS

ma_pagecache_consist_1kWR_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_1kWR_t_CPPFLAGS = 	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=1024 -DTEST_WRITERS
ma_pagecache_consist_64kWR_t_SOURCES =	$(ma_pagecache_consist_src)
ma_pagecache_consist_64kWR_t_CPPFLAGS =	$(ma_pagecache_common_cppflags) -DTEST_PAGE_SIZE=65536 -DTEST_WRITERS

ma_pagecache_rwconsist_1k_t_SOURCES =	ma_pagecache_rwconsist.c
ma_pagecache_rwconsist_1k_t_CPPFLAGS = -DTEST_PAGE_SIZE=1024
ma_pagecache_rwconsist2_1k_t_SOURCES =	ma_pagecache_rwconsist2.c
ma_pagecache_rwconsist2_1k_t_CPPFLAGS = -DTEST_PAGE_SIZE=1024

# the generic lock manager may not be used in the end and lockman1-t crashes,
# and lockman2-t takes at least quarter an hour,
# so we don't build lockman-t and lockman1-t and lockman2-t 
CLEANFILES =		maria_log_control page_cache_test_file_1 \
			maria_log.????????
