# Copyright (C) 2007 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/unittest/mytap)
LINK_LIBRARIES(maria myisam mytap mysys dbug strings wsock32 zlib)

ADD_EXECUTABLE(ma_control_file-t ma_control_file-t.c)
ADD_EXECUTABLE(trnman-t trnman-t.c)
ADD_EXECUTABLE(ma_test_loghandler-t  
	ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_multigroup-t  
	ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c sequence_storage.c)
ADD_EXECUTABLE(ma_test_loghandler_multithread-t  
	ma_test_loghandler_multithread-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_pagecache-t
	ma_test_loghandler_pagecache-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_long-t
	ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
SET_TARGET_PROPERTIES(ma_test_loghandler_long-t  PROPERTIES COMPILE_FLAGS "-DLONG_LOG_TEST")

ADD_EXECUTABLE(ma_test_loghandler_noflush-t 
	ma_test_loghandler_noflush-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_first_lsn-t 
	ma_test_loghandler_first_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_max_lsn-t 
	ma_test_loghandler_max_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_purge-t
	ma_test_loghandler_purge-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_readonly-t
	ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c  sequence_storage.c)
SET_TARGET_PROPERTIES(ma_test_loghandler_readonly-t  PROPERTIES COMPILE_FLAGS "-DREADONLY_TEST")
ADD_EXECUTABLE(ma_test_loghandler_nologs-t
	ma_test_loghandler_nologs-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)

SET(ma_pagecache_single_src	ma_pagecache_single.c test_file.c test_file.h)
SET(ma_pagecache_consist_src ma_pagecache_consist.c test_file.c test_file.h)
SET(ma_pagecache_common_cppflags "-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN")

ADD_EXECUTABLE(ma_pagecache_single_1k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_1k-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024")

ADD_EXECUTABLE(ma_pagecache_single_8k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_8k-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=8192")

ADD_EXECUTABLE(ma_pagecache_single_64k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_64k-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536")

ADD_EXECUTABLE(ma_pagecache_consist_1k-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1k-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024")

ADD_EXECUTABLE(ma_pagecache_consist_64k-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64k-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536")

ADD_EXECUTABLE(ma_pagecache_consist_1kHC-t
	${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kHC-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY")
ADD_EXECUTABLE(ma_pagecache_consist_64kHC-t
	${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kHC-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY")
ADD_EXECUTABLE(ma_pagecache_consist_1kRD-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kRD-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_READERS")
ADD_EXECUTABLE(ma_pagecache_consist_64kRD-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kRD-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_READERS")
ADD_EXECUTABLE(ma_pagecache_consist_1kWR-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kWR-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_WRITERS")
ADD_EXECUTABLE(ma_pagecache_consist_64kWR-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kWR-t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_WRITERS")
ADD_EXECUTABLE(ma_pagecache_rwconsist_1k-t ma_pagecache_rwconsist.c)
SET_TARGET_PROPERTIES(ma_pagecache_rwconsist_1k-t PROPERTIES COMPILE_FLAGS "-DTEST_PAGE_SIZE=1024")
ADD_EXECUTABLE(ma_pagecache_rwconsist2_1k-t ma_pagecache_rwconsist2.c)
SET_TARGET_PROPERTIES(ma_pagecache_rwconsist2_1k-t PROPERTIES COMPILE_FLAGS "-DTEST_PAGE_SIZE=1024")
