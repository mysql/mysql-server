# Copyright (C) 2009 Oracle/Innobase Oy
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

# This is the CMakeLists for InnoDB Plugin

INCLUDE(CheckFunctionExists)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckCSourceRuns)

# OS tests
IF(UNIX)
  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ADD_DEFINITIONS("-DUNIV_LINUX")
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

# Solaris atomics
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  CHECK_FUNCTION_EXISTS(atomic_cas_ulong  HAVE_ATOMIC_CAS_ULONG)
  CHECK_FUNCTION_EXISTS(atomic_cas_32 HAVE_ATOMIC_CAS_32)
  CHECK_FUNCTION_EXISTS(atomic_cas_64 HAVE_ATOMIC_CAS_64)
  CHECK_FUNCTION_EXISTS(atomic_add_long HAVE_ATOMIC_ADD_LONG)
  IF(HAVE_ATOMIC_CAS_ULONG AND HAVE_ATOMIC_CAS_32 AND
    HAVE_ATOMIC_CAS_64 AND HAVE_ATOMIC_ADD_LONG)
	SET(HAVE_SOLARIS_ATOMICS 1)
  ENDIF()
ENDIF()

IF(HAVE_GCC_ATOMIC_BUILTINS)
  CHECK_C_SOURCE_COMPILES("
    #include <pthread.h>
    #include <string.h>
    int main()
    {
      pthread_t       x1;
      pthread_t       x2;
      pthread_t       x3;
      __sync_bool_compare_and_swap(&x1, x2, x3);
      return 0;
    }" HAVE_ATOMIC_PTHREAD_T_GCC
  )
ENDIF()

IF(NOT HAVE_ATOMIC_PTHREAD_T_GCC AND HAVE_SOLARIS_ATOMICS)
  CHECK_C_SOURCE_COMPILES("
  #include <pthread.h>
  int main()
  {
    pthread_t x = 0;
	return(0);
  }" HAVE_ATOMIC_PTHREAD_T_SOLARIS
  )
  IF(HAVE_ATOMIC_PTHREAD_T_SOLARIS)
    SET(CMAKE_EXTRA_INCLUDE_FILES pthread.h)
    CHECK_TYPE_SIZE(pthread_t SIZEOF_PTHREAD_T)
    SET(CMAKE_EXTRA_INCLUDE_FILES)
  ENDIF()
ENDIF()

IF(NOT CMAKE_CROSSCOMPILING)
  STRING(TOLOWER ${CMAKE_SYSTEM_PROCESSOR}  processor)
  IF(NOT WIN32 AND processor MATCHES "86" OR processor MATCHES "amd64" 
     OR processor MATCHES "x64")
  # Check for x86 PAUSE instruction
  # We have to actually try running the test program, because of a bug
  # in Solaris on x86_64, where it wrongly reports that PAUSE is not
  # supported when trying to run an application.  See
  # http://bugs.opensolaris.org/bugdatabase/printableBug.do?bug_id=6478684
  CHECK_C_SOURCE_RUNS("
    int main()
	{ 
      __asm__ __volatile__ (\"pause\"); 
      return 0;
    }"  IB_HAVE_PAUSE_INSTRUCTION)
  ENDIF()
ENDIF()
 
IF(HAVE_ATOMIC_PTHREAD_T_SOLARIS OR HAVE_ATOMIC_PTHREAD_T_GCC)
  ADD_DEFINITIONS(-DHAVE_ATOMIC_PTHREAD_T=1)
ENDIF()
 
IF(HAVE_SOLARIS_ATOMICS)
  ADD_DEFINITIONS(-DHAVE_SOLARIS_ATOMICS=1)
ENDIF()
 
IF(IB_HAVE_PAUSE_INSTRUCTIONS)
  ADD_DEFINITIONS(-DIB_HAVE_PAUSE_INSTRUCTIONS=1)
ENDIF()

IF(SIZEOF_PTHREAD_T)
  ADD_DEFINITIONS(-DSIZEOF_PTHREAD_T=${SIZEOF_PTHREAD_T})
ENDIF()

IF(MSVC)
 ADD_DEFINITIONS(-DHAVE_WINDOWS_ATOMICS -DIB_HAVE_PAUSE_INSTRUCTION)
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
IF (MSVC AND CMAKE_SIZEOF_VOIDP EQUAL 8)
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
			handler/ha_innodb.cc handler/handler0alter.cc handler/i_s.cc handler/mysql_addons.cc
			read/read0read.c
			rem/rem0cmp.c rem/rem0rec.c
			row/row0ext.c row/row0ins.c row/row0merge.c row/row0mysql.c row/row0purge.c row/row0row.c
			row/row0sel.c row/row0uins.c row/row0umod.c row/row0undo.c row/row0upd.c row/row0vers.c
			srv/srv0que.c srv/srv0srv.c srv/srv0start.c
			sync/sync0arr.c sync/sync0rw.c sync/sync0sync.c
			thr/thr0loc.c
			trx/trx0i_s.c trx/trx0purge.c trx/trx0rec.c trx/trx0roll.c trx/trx0rseg.c
			trx/trx0sys.c trx/trx0trx.c trx/trx0undo.c
			usr/usr0sess.c
			ut/ut0byte.c ut/ut0dbg.c ut/ut0mem.c ut/ut0rnd.c ut/ut0ut.c ut/ut0vec.c
			ut/ut0list.c ut/ut0wqueue.c)

IF(WITH_INNODB)
  # Legacy option
  SET(WITH_INNOBASE_STORAGE_ENGINE TRUE)
ENDIF()

SET(INNOBASE_PLUGIN_STATIC  "innobase")
SET(INNOBASE_PLUGIN_DYNAMIC "ha_innodb")

# Innobase depends on zlib. If server links with system
# zlib shared library, and innobase builds as shared library,
# innobase need to link with it too, to avoid unresolved symbols.
IF(ZLIB_FOUND AND NOT WITH_ZLIB STREQUAL "bundled")
	SET(INNOBASE_LIBS ${ZLIB_LIBRARY})
ENDIF()

#The plugin's CMakeLists.txt still needs to work with previous versions of MySQL.
IF(EXISTS ${SOURCE_DIR}/storage/mysql_storage_engine.cmake)
	# Old plugin support on Windows only, 
	# use tricks to force ha_innodb.dll name for DLL
	INCLUDE(${SOURCE_DIR}/storage/mysql_storage_engine.cmake)
	MYSQL_STORAGE_ENGINE(INNOBASE)
	GET_TARGET_PROPERTY(LIB_LOCATION ha_innobase LOCATION)
	IF(LIB_LOCATION)
		SET_TARGET_PROPERTIES(ha_innobase PROPERTIES OUTPUT_NAME ha_innodb)
	ENDIF()
ELSEIF (MYSQL_VERSION_ID LESS "50137")
	# Windows only, no plugin support
	IF (NOT SOURCE_SUBLIBS)
		ADD_DEFINITIONS(-DMYSQL_SERVER)
		ADD_LIBRARY(innobase STATIC ${INNOBASE_SOURCES})
		# Require mysqld_error.h, which is built as part of the GenError
		ADD_DEPENDENCIES(innobase GenError)
	ENDIF()
ELSE()
	# New plugin support, cross-platform , name for shared library
	# is given in INNOBASE_PLUGIN_STATIC and INNOBASE_PLUGIN_DYNAMIC
	MYSQL_STORAGE_ENGINE(INNOBASE)
ENDIF()
