INCLUDES =		@ZLIB_INCLUDES@ -I$(top_builddir)/include \
			-I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap 


LDADD 		= $(top_builddir)/unittest/mytap/libmytap.a \
		  $(top_builddir)/mysys/libmysys.a \
		  $(top_builddir)/dbug/libdbug.a \
		  $(top_builddir)/strings/libmystrings.a

noinst_PROGRAMS  =	bitmap-t base64-t my_atomic-t \
			mf_pagecache_single_1k-t mf_pagecache_single_8k-t \
			mf_pagecache_single_64k-t \
			mf_pagecache_consist_1k-t mf_pagecache_consist_64k-t \
			mf_pagecache_consist_1kHC-t mf_pagecache_consist_64kHC-t \
			mf_pagecache_consist_1kRD-t mf_pagecache_consist_64kRD-t \
			mf_pagecache_consist_1kWR-t mf_pagecache_consist_64kWR-t

# tests for mysys/mf_pagecache.c

mf_pagecache_single_src =	mf_pagecache_single.c $(top_srcdir)/mysys/mf_pagecache.c test_file.c
mf_pagecache_consist_src =	mf_pagecache_consist.c $(top_srcdir)/mysys/mf_pagecache.c test_file.c
mf_pagecache_common_cppflags =	-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN

mf_pagecache_single_1k_t_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_8k_t_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_64k_t_SOURCES =	$(mf_pagecache_single_src)
mf_pagecache_single_1k_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024
mf_pagecache_single_8k_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=8192
mf_pagecache_single_64k_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536

mf_pagecache_consist_1k_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1k_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024
mf_pagecache_consist_64k_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64k_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536

mf_pagecache_consist_1kHC_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kHC_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY
mf_pagecache_consist_64kHC_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kHC_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY

mf_pagecache_consist_1kRD_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kRD_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_READERS
mf_pagecache_consist_64kRD_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kRD_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_READERS

mf_pagecache_consist_1kWR_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_1kWR_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=1024 -DTEST_WRITERS
mf_pagecache_consist_64kWR_t_SOURCES =	$(mf_pagecache_consist_src)
mf_pagecache_consist_64kWR_t_CPPFLAGS = 	$(mf_pagecache_common_cppflags) -DPAGE_SIZE=65536 -DTEST_WRITERS

CLEANFILES = 	my_pagecache_debug.log page_cache_test_file_1
