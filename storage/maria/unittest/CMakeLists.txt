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

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/unittest/mytap)
LINK_LIBRARIES(maria myisam mytap mysys dbug strings wsock32 zlib)

ADD_EXECUTABLE(ma_test_loghandler_t  
	ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_multigroup_t  
	ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_multithread_t  
	ma_test_loghandler_multithread-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_pagecache_t
	ma_test_loghandler_pagecache-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_long_t_big
	ma_test_loghandler-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
SET_TARGET_PROPERTIES(ma_test_loghandler_long_t_big  PROPERTIES COMPILE_FLAGS "-DLONG_LOG_TEST")

ADD_EXECUTABLE(ma_test_loghandler_noflush_t 
	ma_test_loghandler_noflush-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_first_lsn_t 
	ma_test_loghandler_first_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_max_lsn_t 
	ma_test_loghandler_max_lsn-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_purge_t
	ma_test_loghandler_purge-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
ADD_EXECUTABLE(ma_test_loghandler_readonly_t
	ma_test_loghandler_multigroup-t.c ma_maria_log_cleanup.c ma_loghandler_examples.c)
SET_TARGET_PROPERTIES(ma_test_loghandler_readonly_t  PROPERTIES COMPILE_FLAGS "-DREADONLY_TEST")


SET(ma_pagecache_single_src	ma_pagecache_single.c test_file.c test_file.h)
SET(ma_pagecache_consist_src 	ma_pagecache_consist.c test_file.c test_file.h)
SET(ma_pagecache_common_cppflags "-DEXTRA_DEBUG -DPAGECACHE_DEBUG -DMAIN")

ADD_EXECUTABLE(ma_pagecache_single_1k_t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_1k_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=1024")

ADD_EXECUTABLE(ma_pagecache_single_8k_t ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_8k_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=8192")

ADD_EXECUTABLE(ma_pagecache_single_64k_t_big ${ma_pagecache_single_src})
SET_TARGET_PROPERTIES(ma_pagecache_single_64k_t_big
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=65536")

ADD_EXECUTABLE(ma_pagecache_consist_1k_t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1k_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=1024")

ADD_EXECUTABLE(ma_pagecache_consist_64k_t_big ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64k_t_big
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=65536")

ADD_EXECUTABLE(ma_pagecache_consist_1kHC_t
	${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kHC_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=1024 -DTEST_HIGH_CONCURENCY")
ADD_EXECUTABLE(ma_pagecache_consist_64kHC_t_big
	${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kHC_t_big
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=65536 -DTEST_HIGH_CONCURENCY")
ADD_EXECUTABLE(ma_pagecache_consist_1kRD_t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kRD_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=1024 -DTEST_READERS")
ADD_EXECUTABLE(ma_pagecache_consist_64kRD_t_big ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kRD_t_big
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=65536 -DTEST_READERS")
ADD_EXECUTABLE(ma_pagecache_consist_1kWR_t ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_1kWR_t
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=1024 -DTEST_WRITERS")
ADD_EXECUTABLE(ma_pagecache_consist_64kWR_t_big ${ma_pagecache_consist_src})
SET_TARGET_PROPERTIES(ma_pagecache_consist_64kWR_t_big
	PROPERTIES COMPILE_FLAGS "${ma_pagecache_common_cppflags} -DPAGE_SIZE=65536 -DTEST_WRITERS")






