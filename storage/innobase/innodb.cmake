# Copyright (c) 2006, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This is the CMakeLists for InnoDB

INCLUDE(CheckFunctionExists)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckCSourceRuns)

IF(LZ4_LIBRARY)
  ADD_DEFINITIONS(-DHAVE_LZ4=1)
ENDIF()

#ADD_DEFINITIONS("-O0")

# OS tests
IF(UNIX AND NOT IGNORE_AIO_CHECK)
  IF(LINUX)

    ADD_DEFINITIONS("-DUNIV_LINUX")

    CHECK_INCLUDE_FILES (libaio.h HAVE_LIBAIO_H)
    CHECK_LIBRARY_EXISTS(aio io_queue_init "" HAVE_LIBAIO)

    IF(HAVE_LIBAIO_H AND HAVE_LIBAIO)
      ADD_DEFINITIONS(-DLINUX_NATIVE_AIO=1)
      LINK_LIBRARIES(aio)
    ENDIF()

  ELSEIF(SOLARIS)
    ADD_DEFINITIONS("-DUNIV_SOLARIS")
  ENDIF()
ENDIF()

OPTION(INNODB_COMPILER_HINTS "Compile InnoDB with compiler hints" ON)
MARK_AS_ADVANCED(INNODB_COMPILER_HINTS)

IF(INNODB_COMPILER_HINTS)
   ADD_DEFINITIONS("-DCOMPILER_HINTS")
ENDIF()

SET(MUTEXTYPE "event" CACHE STRING "Mutex type: event, sys or futex")

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  # Turn off unused parameter warnings.
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-parameter")
  # Turn off warnings about implicit casting away const.
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-cast-qual")
ENDIF()

IF(MY_COMPILER_IS_GNU)
  # Add -Wconversion if compiling with GCC
  ## As of Mar 15 2011 this flag causes 3573+ warnings. If you are reading this
  ## please fix them and enable the following code:
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")

  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    INCLUDE(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-fno-builtin-memcmp" HAVE_NO_BUILTIN_MEMCMP)
    IF (HAVE_NO_BUILTIN_MEMCMP)
      # Work around http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43052
      SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/rem/rem0cmp.cc
        PROPERTIES COMPILE_FLAGS -fno-builtin-memcmp)
    ENDIF()
  ENDIF()
ENDIF()

# Enable InnoDB's UNIV_DEBUG in debug builds
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DUNIV_DEBUG")

OPTION(WITH_INNODB_EXTRA_DEBUG "Enable extra InnoDB debug checks" OFF)
IF(WITH_INNODB_EXTRA_DEBUG)
  IF(NOT WITH_DEBUG)
    MESSAGE(FATAL_ERROR "WITH_INNODB_EXTRA_DEBUG can be enabled only when WITH_DEBUG is enabled")
  ENDIF()

  SET(EXTRA_DEBUG_FLAGS "")
  SET(EXTRA_DEBUG_FLAGS "${EXTRA_DEBUG_FLAGS} -DUNIV_AHI_DEBUG")
  SET(EXTRA_DEBUG_FLAGS "${EXTRA_DEBUG_FLAGS} -DUNIV_DDL_DEBUG")
  SET(EXTRA_DEBUG_FLAGS "${EXTRA_DEBUG_FLAGS} -DUNIV_DEBUG_FILE_ACCESSES")
  SET(EXTRA_DEBUG_FLAGS "${EXTRA_DEBUG_FLAGS} -DUNIV_ZIP_DEBUG")

  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${EXTRA_DEBUG_FLAGS}")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${EXTRA_DEBUG_FLAGS}")
ENDIF()

CHECK_FUNCTION_EXISTS(sched_getcpu  HAVE_SCHED_GETCPU)
IF(HAVE_SCHED_GETCPU)
 ADD_DEFINITIONS(-DHAVE_SCHED_GETCPU=1)
ENDIF()

CHECK_FUNCTION_EXISTS(nanosleep HAVE_NANOSLEEP)
IF(HAVE_NANOSLEEP)
 ADD_DEFINITIONS(-DHAVE_NANOSLEEP=1)
ENDIF()

IF(NOT MSVC)
  CHECK_C_SOURCE_RUNS(
  "
  #ifndef _GNU_SOURCE
  #define _GNU_SOURCE
  #endif
  #include <fcntl.h>
  #include <linux/falloc.h>
  int main()
  {
    /* Ignore the return value for now. Check if the flags exist.
    The return value is checked  at runtime. */
    fallocate(0, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, 0);

    return(0);
  }"
  HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
  )
  CHECK_C_SOURCE_COMPILES(
  "
  #ifndef _GNU_SOURCE
  #define _GNU_SOURCE
  #endif
  #include <fcntl.h>
  #include <linux/falloc.h>
  int main()
  {
    /* Ignore the return value for now. Check if the flags exist.
    The return value is checked  at runtime. */
    fallocate(0, FALLOC_FL_ZERO_RANGE, 0, 0);

    return(0);
  }"
  HAVE_FALLOC_FL_ZERO_RANGE
  )
ENDIF()

IF(HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE)
 ADD_DEFINITIONS(-DHAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE=1)
ENDIF()

IF(HAVE_FALLOC_FL_ZERO_RANGE)
 ADD_DEFINITIONS(-DHAVE_FALLOC_FL_ZERO_RANGE=1)
ENDIF()

IF(NOT MSVC)
IF(NOT CMAKE_CROSSCOMPILING)
  CHECK_C_SOURCE_RUNS(
  "#include<stdint.h>
  int main()
  {
    __sync_synchronize();
    return(0);
  }"
  HAVE_IB_GCC_SYNC_SYNCHRONISE
  )
  CHECK_C_SOURCE_RUNS(
  "#include<stdint.h>
  int main()
  {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    return(0);
  }"
  HAVE_IB_GCC_ATOMIC_THREAD_FENCE
  )
ENDIF()

IF(HAVE_IB_GCC_SYNC_SYNCHRONISE)
 ADD_DEFINITIONS(-DHAVE_IB_GCC_SYNC_SYNCHRONISE=1)
ENDIF()

IF(HAVE_IB_GCC_ATOMIC_THREAD_FENCE)
 ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_THREAD_FENCE=1)
ENDIF()

# Only use futexes on Linux if GCC atomics are available
IF(NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
  CHECK_C_SOURCE_RUNS(
  "
  #include <stdio.h>
  #include <unistd.h>
  #include <errno.h>
  #include <assert.h>
  #include <linux/futex.h>
  #include <unistd.h>
  #include <sys/syscall.h>

   int futex_wait(int* futex, int v) {
        return(syscall(SYS_futex, futex, FUTEX_WAIT_PRIVATE, v, NULL, NULL, 0));
   }

   int futex_signal(int* futex) {
        return(syscall(SYS_futex, futex, FUTEX_WAKE, 1, NULL, NULL, 0));
   }

  int main() {
        int     ret;
        int     m = 1;

        /* It is setup to fail and return EWOULDBLOCK. */
        ret = futex_wait(&m, 0);
        assert(ret == -1 && errno == EWOULDBLOCK);
        /* Shouldn't wake up any threads. */
        assert(futex_signal(&m) == 0);

        return(0);
  }"
  HAVE_IB_LINUX_FUTEX)
ENDIF()
IF(HAVE_IB_LINUX_FUTEX)
  ADD_DEFINITIONS(-DHAVE_IB_LINUX_FUTEX=1)
ENDIF()

ENDIF(NOT MSVC)

# Solaris atomics
IF(SOLARIS)
  IF(NOT CMAKE_CROSSCOMPILING)
  CHECK_C_SOURCE_COMPILES(
  "#include <mbarrier.h>
  int main() {
    __machine_r_barrier();
    __machine_w_barrier();
    return(0);
  }"
  HAVE_IB_MACHINE_BARRIER_SOLARIS)
  ENDIF()
  IF(HAVE_IB_MACHINE_BARRIER_SOLARIS)
    ADD_DEFINITIONS(-DHAVE_IB_MACHINE_BARRIER_SOLARIS=1)
  ENDIF()
ENDIF()

IF(MSVC)
  ADD_DEFINITIONS(-DHAVE_WINDOWS_MM_FENCE)
ENDIF()

IF(MUTEXTYPE MATCHES "event")
  ADD_DEFINITIONS(-DMUTEX_EVENT)
ELSEIF(MUTEXTYPE MATCHES "futex" AND DEFINED HAVE_IB_LINUX_FUTEX)
  ADD_DEFINITIONS(-DMUTEX_FUTEX)
ELSE()
   ADD_DEFINITIONS(-DMUTEX_SYS)
ENDIF()

# Include directories under innobase
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/innobase/
                    ${CMAKE_SOURCE_DIR}/storage/innobase/include
                    ${CMAKE_SOURCE_DIR}/storage/innobase/handler
                    )
