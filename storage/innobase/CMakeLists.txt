# Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# This is the CMakeLists for InnoDB

INCLUDE(CheckFunctionExists)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckCSourceRuns)

# OS tests
IF(UNIX)
  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    CHECK_INCLUDE_FILES (libaio.h HAVE_LIBAIO_H)
    CHECK_LIBRARY_EXISTS(aio io_queue_init "" HAVE_LIBAIO)
    ADD_DEFINITIONS("-DUNIV_LINUX -D_GNU_SOURCE=1")
    IF(HAVE_LIBAIO_H AND HAVE_LIBAIO)
      ADD_DEFINITIONS(-DLINUX_NATIVE_AIO=1)
      LINK_LIBRARIES(aio)
    ENDIF()
  ELSEIF(CMAKE_SYSTEM_NAME MATCHES "HP*")
    ADD_DEFINITIONS("-DUNIV_HPUX")
  ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    ADD_DEFINITIONS("-DUNIV_AIX")
  ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    ADD_DEFINITIONS("-DUNIV_SOLARIS")
  ENDIF()
ENDIF()

IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
# After: WL#5825 Using C++ Standard Library with MySQL code
#       we no longer use -fno-exceptions
#	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
ENDIF()

# Enable InnoDB's UNIV_DEBUG and UNIV_SYNC_DEBUG in debug builds
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DUNIV_DEBUG -DUNIV_SYNC_DEBUG")

# Add -Wconversion if compiling with GCC
## As of Mar 15 2011 this flag causes 3573+ warnings. If you are reading this
## please fix them and enable the following code:
#IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")
#ENDIF()

CHECK_FUNCTION_EXISTS(sched_getcpu  HAVE_SCHED_GETCPU)

IF(NOT MSVC)
# either define HAVE_IB_GCC_ATOMIC_BUILTINS or not
IF(NOT CMAKE_CROSSCOMPILING)
  CHECK_C_SOURCE_RUNS(
  "
  int main()
  {
    long	x;
    long	y;
    long	res;
    char	c;

    x = 10;
    y = 123;
    res = __sync_bool_compare_and_swap(&x, x, y);
    if (!res || x != y) {
      return(1);
    }

    x = 10;
    y = 123;
    res = __sync_bool_compare_and_swap(&x, x + 1, y);
    if (res || x != 10) {
      return(1);
    }
    x = 10;
    y = 123;
    res = __sync_add_and_fetch(&x, y);
    if (res != 123 + 10 || x != 123 + 10) {
      return(1);
    }

    c = 10;
    res = __sync_lock_test_and_set(&c, 123);
    if (res != 10 || c != 123) {
      return(1);
    }
    return(0);
  }"
  HAVE_IB_GCC_ATOMIC_BUILTINS
  )
  CHECK_C_SOURCE_RUNS(
  "#include<stdint.h>
  int main()
  {
    int64_t	x,y,res;

    x = 10;
    y = 123;
    res = __sync_sub_and_fetch(&y, x);
    if (res != y || y != 113) {
      return(1);
    }
    res = __sync_add_and_fetch(&y, x);
    if (res != y || y != 123) {
      return(1);
    }
    return(0);
  }"
  HAVE_IB_GCC_ATOMIC_BUILTINS_64
  )
ENDIF()

IF(HAVE_IB_GCC_ATOMIC_BUILTINS)
 ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_BUILTINS=1)
ENDIF()

IF(HAVE_IB_GCC_ATOMIC_BUILTINS_64)
 ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_BUILTINS_64=1)
ENDIF()

 # either define HAVE_IB_ATOMIC_PTHREAD_T_GCC or not
IF(NOT CMAKE_CROSSCOMPILING)
  CHECK_C_SOURCE_RUNS(
  "
  #include <pthread.h>
  #include <string.h>

  int main() {
    pthread_t       x1;
    pthread_t       x2;
    pthread_t       x3;

    memset(&x1, 0x0, sizeof(x1));
    memset(&x2, 0x0, sizeof(x2));
    memset(&x3, 0x0, sizeof(x3));

    __sync_bool_compare_and_swap(&x1, x2, x3);

    return(0);
  }"
  HAVE_IB_ATOMIC_PTHREAD_T_GCC)
ENDIF()
IF(HAVE_IB_ATOMIC_PTHREAD_T_GCC)
  ADD_DEFINITIONS(-DHAVE_IB_ATOMIC_PTHREAD_T_GCC=1)
ENDIF()

ENDIF(NOT MSVC)

CHECK_FUNCTION_EXISTS(asprintf  HAVE_ASPRINTF)
CHECK_FUNCTION_EXISTS(vasprintf  HAVE_VASPRINTF)

# Solaris atomics
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  CHECK_FUNCTION_EXISTS(atomic_cas_ulong  HAVE_ATOMIC_CAS_ULONG)
  CHECK_FUNCTION_EXISTS(atomic_cas_32 HAVE_ATOMIC_CAS_32)
  CHECK_FUNCTION_EXISTS(atomic_cas_64 HAVE_ATOMIC_CAS_64)
  CHECK_FUNCTION_EXISTS(atomic_add_long_nv HAVE_ATOMIC_ADD_LONG_NV)
  CHECK_FUNCTION_EXISTS(atomic_swap_uchar HAVE_ATOMIC_SWAP_UCHAR)
  IF(HAVE_ATOMIC_CAS_ULONG AND
     HAVE_ATOMIC_CAS_32 AND
     HAVE_ATOMIC_CAS_64 AND
     HAVE_ATOMIC_ADD_LONG_NV AND
     HAVE_ATOMIC_SWAP_UCHAR)
    SET(HAVE_IB_SOLARIS_ATOMICS 1)
  ENDIF()

  IF(HAVE_IB_SOLARIS_ATOMICS)
    ADD_DEFINITIONS(-DHAVE_IB_SOLARIS_ATOMICS=1)
  ENDIF()

  IF(NOT CMAKE_CROSSCOMPILING)
  # either define HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS or not
  CHECK_C_SOURCE_COMPILES(
  "   #include <pthread.h>
      #include <string.h>

      int main(int argc, char** argv) {
        pthread_t       x1;
        pthread_t       x2;
        pthread_t       x3;

        memset(&x1, 0x0, sizeof(x1));
        memset(&x2, 0x0, sizeof(x2));
        memset(&x3, 0x0, sizeof(x3));

        if (sizeof(pthread_t) == 4) {

          atomic_cas_32(&x1, x2, x3);

        } else if (sizeof(pthread_t) == 8) {

          atomic_cas_64(&x1, x2, x3);

        } else {

          return(1);
        }

      return(0);
    }
  " HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS)
  ENDIF()
  IF(HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS)
    ADD_DEFINITIONS(-DHAVE_IB_ATOMIC_PTHREAD_T_SOLARIS=1)
  ENDIF()
ENDIF()


IF(UNIX)
# this is needed to know which one of atomic_cas_32() or atomic_cas_64()
# to use in the source
SET(CMAKE_EXTRA_INCLUDE_FILES pthread.h)
CHECK_TYPE_SIZE(pthread_t SIZEOF_PTHREAD_T)
SET(CMAKE_EXTRA_INCLUDE_FILES)
ENDIF()

IF(SIZEOF_PTHREAD_T)
  ADD_DEFINITIONS(-DSIZEOF_PTHREAD_T=${SIZEOF_PTHREAD_T})
ENDIF()

IF(MSVC)
  ADD_DEFINITIONS(-DHAVE_WINDOWS_ATOMICS)
ENDIF()


# Include directories under innobase
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/innobase/include
		    ${CMAKE_SOURCE_DIR}/storage/innobase/handler)

# Sun Studio bug with -xO2
IF(CMAKE_CXX_COMPILER_ID MATCHES "SunPro"
	AND CMAKE_CXX_FLAGS_RELEASE MATCHES "O2"
	AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	# Sun Studio 12 crashes with -xO2 flag, but not with higher optimization
	# -xO3
	SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/rem/rem0rec.cc
    PROPERTIES COMPILE_FLAGS -xO3)
ENDIF()

# Removing compiler optimizations for innodb/mem/* files on 64-bit Windows
# due to 64-bit compiler error, See MySQL Bug #19424, #36366, #34297
IF (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET_SOURCE_FILES_PROPERTIES(mem/mem0mem.cc mem/mem0pool.cc
				    PROPERTIES COMPILE_FLAGS -Od)
ENDIF()

SET(INNOBASE_SOURCES
	api/api0api.cc
	api/api0misc.cc
	btr/btr0btr.cc
	btr/btr0cur.cc
	btr/btr0pcur.cc
	btr/btr0sea.cc
	buf/buf0buddy.cc
	buf/buf0buf.cc
	buf/buf0dblwr.cc
	buf/buf0checksum.cc
	buf/buf0dump.cc
	buf/buf0flu.cc
	buf/buf0lru.cc
	buf/buf0rea.cc
	data/data0data.cc
	data/data0type.cc
	dict/dict0boot.cc
	dict/dict0crea.cc
	dict/dict0dict.cc
	dict/dict0load.cc
	dict/dict0mem.cc
	dict/dict0stats.cc
	dict/dict0stats_bg.cc
	dyn/dyn0dyn.cc
	eval/eval0eval.cc
	eval/eval0proc.cc
	fil/fil0fil.cc
	fsp/fsp0fsp.cc
	fut/fut0fut.cc
	fut/fut0lst.cc
	ha/ha0ha.cc
	ha/ha0storage.cc
	ha/hash0hash.cc
	fts/fts0fts.cc
	fts/fts0ast.cc
	fts/fts0blex.cc
	fts/fts0config.cc
	fts/fts0opt.cc
	fts/fts0pars.cc
	fts/fts0que.cc
	fts/fts0sql.cc
	fts/fts0tlex.cc
	handler/ha_innodb.cc
	handler/handler0alter.cc
	handler/i_s.cc
	ibuf/ibuf0ibuf.cc
	lock/lock0iter.cc
	lock/lock0lock.cc
	lock/lock0wait.cc
	log/log0log.cc
	log/log0recv.cc
	mach/mach0data.cc
	mem/mem0mem.cc
	mem/mem0pool.cc
	mtr/mtr0log.cc
	mtr/mtr0mtr.cc
	os/os0file.cc
	os/os0proc.cc
	os/os0sync.cc
	os/os0thread.cc
	page/page0cur.cc
	page/page0page.cc
	page/page0zip.cc
	pars/lexyy.cc
	pars/pars0grm.cc
	pars/pars0opt.cc
	pars/pars0pars.cc
	pars/pars0sym.cc
	que/que0que.cc
	read/read0read.cc
	rem/rem0cmp.cc
	rem/rem0rec.cc
	row/row0ext.cc
	row/row0ftsort.cc
	row/row0import.cc
	row/row0ins.cc
	row/row0merge.cc
	row/row0mysql.cc
	row/row0log.cc
	row/row0purge.cc
	row/row0row.cc
	row/row0sel.cc
	row/row0uins.cc
	row/row0umod.cc
	row/row0undo.cc
	row/row0upd.cc
	row/row0quiesce.cc
	row/row0vers.cc
	srv/srv0conc.cc
	srv/srv0mon.cc
	srv/srv0srv.cc
	srv/srv0space.cc
	srv/srv0start.cc
	sync/sync0arr.cc
	sync/sync0rw.cc
	sync/sync0sync.cc
	trx/trx0i_s.cc
	trx/trx0purge.cc
	trx/trx0rec.cc
	trx/trx0roll.cc
	trx/trx0rseg.cc
	trx/trx0sys.cc
	trx/trx0trx.cc
	trx/trx0undo.cc
	usr/usr0sess.cc
	ut/ut0bh.cc
	ut/ut0byte.cc
	ut/ut0crc32.cc
	ut/ut0dbg.cc
	ut/ut0list.cc
	ut/ut0mem.cc
	ut/ut0rbt.cc
	ut/ut0rnd.cc
	ut/ut0ut.cc
	ut/ut0vec.cc
	ut/ut0wqueue.cc)

IF(WITH_INNODB)
  # Legacy option
  SET(WITH_INNOBASE_STORAGE_ENGINE TRUE)
ENDIF()

MYSQL_ADD_PLUGIN(innobase ${INNOBASE_SOURCES} STORAGE_ENGINE
  DEFAULT
  MODULE_OUTPUT_NAME ha_innodb
  LINK_LIBRARIES ${ZLIB_LIBRARY})
