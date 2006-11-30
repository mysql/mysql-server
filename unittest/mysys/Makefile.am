INCLUDES =		@ZLIB_INCLUDES@ -I$(top_builddir)/include \
			-I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap 


LDADD 		= $(top_builddir)/unittest/mytap/libmytap.a \
		  $(top_builddir)/mysys/libmysys.a \
		  $(top_builddir)/dbug/libdbug.a \
		  $(top_builddir)/strings/libmystrings.a

# tests with the "big" suffix are too big to run for every push,
# cause load problems on shared machines. So we build them,
# but don't run them by default (for this, a non "-t" suffix is used).
# You can run them by hand (how: see "test" target in upper dir).
noinst_PROGRAMS  =	bitmap-t base64-t my_atomic-t \
			mf_pagecache_single_1k-t-big mf_pagecache_single_8k-t-big \
			mf_pagecache_single_64k-t-big \
			mf_pagecache_consist_1k-t-big mf_pagecache_consist_64k-t-big \
			mf_pagecache_consist_1kHC-t-big mf_pagecache_consist_64kHC-t-big \
			mf_pagecache_consist_1kRD-t-big mf_pagecache_consist_64kRD-t-big \
			mf_pagecache_consist_1kWR-t-big mf_pagecache_consist_64kWR-t-big

# tests for mysys/mf_pagecache.c

mf_pagecache_single_src =	mf_pagecache_single.c $(top_srcdir)/mysys/mf_pagecache.c test_file.c test_file.h
mf_pagecache_consist_src =	mf_pagecache_consist.c $(top_srcdir)/mysys/mf_pagecache.c test_file.c test_file.h
mf_pagecache_common_cppflags =	-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN

mf_pagecache_single_1k_t_big_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_8k_t_big_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_64k_t_big_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_1k_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024
mf_pagecache_single_8k_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=8192
mf_pagecache_single_64k_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536

mf_pagecache_consist_1k_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1k_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024
mf_pagecache_consist_64k_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64k_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536

mf_pagecache_consist_1kHC_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kHC_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY
mf_pagecache_consist_64kHC_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kHC_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY

mf_pagecache_consist_1kRD_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kRD_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_READERS
mf_pagecache_consist_64kRD_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kRD_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_READERS

mf_pagecache_consist_1kWR_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kWR_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_WRITERS
mf_pagecache_consist_64kWR_t_big_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kWR_t_big_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_WRITERS

CLEANFILES = 	my_pagecache_debug.log page_cache_test_file_1
