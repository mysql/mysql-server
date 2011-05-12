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
    ADD_DEFINITIONS("-DUNIV_HPUX -DUNIV_MUST_NOT_INLINE")
  ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    ADD_DEFINITIONS("-DUNIV_AIX -DUNIX_MUST_NOT_INLINE")
  ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    ADD_DEFINITIONS("-DUNIV_SOLARIS")
  ELSE()
   ADD_DEFINITIONS("-DUNIV_MUST_NOT_INLINE")
  ENDIF()
ENDIF()

# Enable InnoDB's UNIV_DEBUG for debug builds
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUNIV_DEBUG")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DUNIV_DEBUG")

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
ENDIF()

IF(HAVE_IB_GCC_ATOMIC_BUILTINS)
 ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_BUILTINS=1)
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
IF(CMAKE_C_COMPILER_ID MATCHES "SunPro" 
	AND CMAKE_C_FLAGS_RELEASE MATCHES "O2" 
	AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	# Sun Studio 12 crashes with -xO2 flag, but not with higher optimization
	# -xO3
	SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/rem/rem0rec.c 
    PROPERTIES COMPILE_FLAGS -xO3)
ENDIF()

# Removing compiler optimizations for innodb/mem/* files on 64-bit Windows
# due to 64-bit compiler error, See MySQL Bug #19424, #36366, #34297
IF (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET_SOURCE_FILES_PROPERTIES(mem/mem0mem.c mem/mem0pool.c
				    PROPERTIES COMPILE_FLAGS -Od)
ENDIF()

SET(INNOBASE_SOURCES	btr/btr0btr.c btr/btr0cur.c btr/btr0pcur.c btr/btr0sea.c
			buf/buf0buddy.c buf/buf0buf.c buf/buf0flu.c buf/buf0lru.c buf/buf0rea.c
			data/data0data.c data/data0type.c
			dict/dict0boot.c dict/dict0crea.c dict/dict0dict.c dict/dict0load.c dict/dict0mem.c
			dyn/dyn0dyn.c
			eval/eval0eval.c eval/eval0proc.c
			fil/fil0fil.c
			fsp/fsp0fsp.c
			fut/fut0fut.c fut/fut0lst.c
			ha/ha0ha.c ha/hash0hash.c ha/ha0storage.c
			ibuf/ibuf0ibuf.c
			pars/lexyy.c pars/pars0grm.c pars/pars0opt.c pars/pars0pars.c pars/pars0sym.c
			lock/lock0lock.c lock/lock0iter.c
			log/log0log.c log/log0recv.c
			mach/mach0data.c
			mem/mem0mem.c mem/mem0pool.c
			mtr/mtr0log.c mtr/mtr0mtr.c
			os/os0file.c os/os0proc.c os/os0sync.c os/os0thread.c
			page/page0cur.c page/page0page.c page/page0zip.c
			que/que0que.c
			handler/ha_innodb.cc handler/handler0alter.cc handler/i_s.cc
			read/read0read.c
			rem/rem0cmp.c rem/rem0rec.c
			row/row0ext.c row/row0ins.c row/row0merge.c row/row0mysql.c row/row0purge.c row/row0row.c
			row/row0sel.c row/row0uins.c row/row0umod.c row/row0undo.c row/row0upd.c row/row0vers.c
			srv/srv0srv.c srv/srv0start.c
			sync/sync0arr.c sync/sync0rw.c sync/sync0sync.c
			trx/trx0i_s.c trx/trx0purge.c trx/trx0rec.c trx/trx0roll.c trx/trx0rseg.c
			trx/trx0sys.c trx/trx0trx.c trx/trx0undo.c
			usr/usr0sess.c
			ut/ut0byte.c ut/ut0dbg.c ut/ut0list.c ut/ut0mem.c ut/ut0rbt.c ut/ut0rnd.c
			ut/ut0ut.c ut/ut0vec.c ut/ut0wqueue.c ut/ut0bh.c)

IF(WITH_INNODB)
  # Legacy option
  SET(WITH_INNOBASE_STORAGE_ENGINE TRUE)
ENDIF()

MYSQL_ADD_PLUGIN(innobase ${INNOBASE_SOURCES} STORAGE_ENGINE 
  DEFAULT
  MODULE_OUTPUT_NAME ha_innodb
  LINK_LIBRARIES ${ZLIB_LIBRARY})
