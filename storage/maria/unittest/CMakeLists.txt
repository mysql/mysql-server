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
LINK_LIBRARIES(aria myisam mytap mysys dbug strings ${ZLIB_LIBRARY})

MY_ADD_TESTS(ma_control_file trnman)

ADD_EXECUTABLE(ma_test_loghandler-t  
        ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler)

ADD_EXECUTABLE(ma_test_loghandler_multigroup-t  
        ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c sequence_storage.c)
MY_ADD_TEST(ma_test_loghandler_multigroup)

ADD_EXECUTABLE(ma_test_loghandler_multithread-t  
        ma_test_loghandler_multithread-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_multithread)

ADD_EXECUTABLE(ma_test_loghandler_pagecache-t
        ma_test_loghandler_pagecache-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_pagecache)

ADD_EXECUTABLE(ma_test_loghandler_long-t
        ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
SET_TARGET_PROPERTIES(ma_test_loghandler_long-t  PROPERTIES COMPILE_FLAGS "-DLONG_LOG_TEST")
MY_ADD_TEST(ma_test_loghandler_long)

ADD_EXECUTABLE(ma_test_loghandler_noflush-t 
        ma_test_loghandler_noflush-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_noflush)

ADD_EXECUTABLE(ma_test_loghandler_first_lsn-t 
        ma_test_loghandler_first_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_first_lsn)

ADD_EXECUTABLE(ma_test_loghandler_max_lsn-t 
        ma_test_loghandler_max_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_max_lsn)

ADD_EXECUTABLE(ma_test_loghandler_purge-t
        ma_test_loghandler_purge-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_purge)

ADD_EXECUTABLE(ma_test_loghandler_readonly-t
        ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c  sequence_storage.c)
MY_ADD_TEST(ma_test_loghandler_readonly)

SET_TARGET_PROPERTIES(ma_test_loghandler_readonly-t  PROPERTIES COMPILE_FLAGS "-DREADONLY_TEST")
ADD_EXECUTABLE(ma_test_loghandler_nologs-t
        ma_test_loghandler_nologs-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
MY_ADD_TEST(ma_test_loghandler_nologs)

SET(ma_pagecache_single_src     ma_pagecache_single.c test_file.c test_file.h)
SET(ma_pagecache_consist_src ma_pagecache_consist.c test_file.c test_file.h)
SET(ma_pagecache_common_cppflags "-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN")

ADD_EXECUTABLE(ma_pagecache_single_1k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_1k-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024")
MY_ADD_TEST(ma_pagecache_single_1k)

ADD_EXECUTABLE(ma_pagecache_single_8k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_8k-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=8192 -DBIG")
MY_ADD_TEST(ma_pagecache_single_8k)

ADD_EXECUTABLE(ma_pagecache_single_64k-t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_64k-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DBIG")
MY_ADD_TEST(ma_pagecache_single_64k)

ADD_EXECUTABLE(ma_pagecache_consist_1k-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1k-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024")
MY_ADD_TEST(ma_pagecache_consist_1k)

ADD_EXECUTABLE(ma_pagecache_consist_64k-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64k-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536")
MY_ADD_TEST(ma_pagecache_consist_64k)

ADD_EXECUTABLE(ma_pagecache_consist_1kHC-t
        ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kHC-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY")
MY_ADD_TEST(ma_pagecache_consist_1kHC)

ADD_EXECUTABLE(ma_pagecache_consist_64kHC-t
        ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kHC-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY")
MY_ADD_TEST(ma_pagecache_consist_64kHC)

ADD_EXECUTABLE(ma_pagecache_consist_1kRD-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kRD-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_READERS")
MY_ADD_TEST(ma_pagecache_consist_1kRD)

ADD_EXECUTABLE(ma_pagecache_consist_64kRD-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kRD-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_READERS")
MY_ADD_TEST(ma_pagecache_consist_64kRD)

ADD_EXECUTABLE(ma_pagecache_consist_1kWR-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kWR-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=1024 -DTEST_WRITERS")
MY_ADD_TEST(ma_pagecache_consist_1kWR)

ADD_EXECUTABLE(ma_pagecache_consist_64kWR-t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kWR-t
        PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DTEST_PAGE_SIZE=65536 -DTEST_WRITERS")
MY_ADD_TEST(ma_pagecache_consist_64kWR)

ADD_EXECUTABLE(ma_pagecache_rwconsist_1k-t ma_pagecache_rwconsist.c)
SET_TARGET_PROPERTIES(ma_pagecache_rwconsist_1k-t PROPERTIES COMPILE_FLAGS "-DTEST_PAGE_SIZE=1024")
MY_ADD_TEST(ma_pagecache_rwconsist_1k)

ADD_EXECUTABLE(ma_pagecache_rwconsist2_1k-t ma_pagecache_rwconsist2.c)
SET_TARGET_PROPERTIES(ma_pagecache_rwconsist2_1k-t PROPERTIES COMPILE_FLAGS "-DTEST_PAGE_SIZE=1024")
MY_ADD_TEST(ma_pagecache_rwconsist2_1k)

