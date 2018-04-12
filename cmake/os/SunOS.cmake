# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(CheckSymbolExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCSourceCompiles) 
INCLUDE(CheckCXXSourceCompiles)

SET(SOLARIS 1)
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  SET(SOLARIS_SPARC 1)
ENDIF()

IF (NOT "${CMAKE_C_FLAGS}${CMAKE_CXX_FLAGS}" MATCHES "-m32|-m64")
  IF(NOT FORCE_UNSUPPORTED_COMPILER)
    MESSAGE("Adding -m64")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
    SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -m64")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -m64")
  ENDIF()
ENDIF()

INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE("void *" SIZEOF_VOIDP)

# We require at least GCC 4.8.3 or SunStudio 12.4 (CC 5.13)
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(CMAKE_COMPILER_IS_GNUCC)
    EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)
    IF(GCC_VERSION VERSION_LESS 4.8.3)
      MESSAGE(FATAL_ERROR "GCC 4.8.3 or newer is required!")
    ENDIF()
  ELSEIF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
    IF(SIZEOF_VOIDP MATCHES 4)
      MESSAGE(FATAL_ERROR "32 bit Solaris builds are not supported. ")
    ENDIF()
    # CC -V yields
    # CC: Studio 12.6 Sun C++ 5.15 SunOS_sparc Beta 2016/12/19
    # CC: Studio 12.5 Sun C++ 5.14 SunOS_sparc Dodona 2016/04/04
    # CC: Sun C++ 5.13 SunOS_sparc Beta 2014/03/11
    # CC: Sun C++ 5.11 SunOS_sparc 2010/08/13
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_CXX_COMPILER} "-V"
      OUTPUT_VARIABLE stdout
      ERROR_VARIABLE  stderr
      RESULT_VARIABLE result
    )
    STRING(REGEX MATCH "CC: Sun C\\+\\+ 5\\.([0-9]+)" VERSION_STRING ${stderr})
    IF (NOT CMAKE_MATCH_1 OR CMAKE_MATCH_1 STREQUAL "")
      STRING(REGEX MATCH "CC: Studio 12\\.[56] Sun C\\+\\+ 5\\.([0-9]+)"
        VERSION_STRING ${stderr})
    ENDIF()
    SET(CC_MINOR_VERSION ${CMAKE_MATCH_1})
    IF(${CC_MINOR_VERSION} LESS 13)
      MESSAGE(FATAL_ERROR "SunStudio 12.4 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# Enable 64 bit file offsets
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

# Enable general POSIX extensions. See standards(5) man page.
ADD_DEFINITIONS(-D__EXTENSIONS__)

# Solaris threads with POSIX semantics:
# http://docs.oracle.com/cd/E19455-01/806-5257/6je9h033k/index.html
ADD_DEFINITIONS(-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT -D_PTHREADS)

# Workaround for Bug 22973151
# Cannot include <math.h> then <cmath> w/ Studio 12.4 in -std=c++11 mode
IF(CMAKE_SYSTEM_VERSION VERSION_EQUAL "5.11" AND CC_MINOR_VERSION EQUAL 13)
  EXEC_PROGRAM(uname ARGS -v OUTPUT_VARIABLE MY_OS_MINOR_VERSION)
  IF(MY_OS_MINOR_VERSION MATCHES "11.3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W0,-stdhdrs_not_idempotent")
    MESSAGE("Adding -W0,-stdhdrs_not_idempotent")
  ENDIF()
ENDIF()


# On  Solaris, use of intrinsics will screw the lib search logic
# Force using -lm, so rint etc are found.
SET(LIBM m)

# CMake defined -lthread as thread flag. This crashes in dlopen 
# when trying to load plugins workaround with -lpthread
SET(CMAKE_THREAD_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# Solaris specific large page support
CHECK_SYMBOL_EXISTS(MHA_MAPSIZE_VA sys/mman.h  HAVE_SOLARIS_LARGE_PAGES)

# Solaris atomics
CHECK_C_SOURCE_RUNS(
 "
 #include  <atomic.h>
  int main()
  {
    int foo = -10; int bar = 10;
    int64_t foo64 = -10; int64_t bar64 = 10;
    if (atomic_add_int_nv((uint_t *)&foo, bar) || foo)
      return -1;
    bar = atomic_swap_uint((uint_t *)&foo, (uint_t)bar);
    if (bar || foo != 10)
     return -1;
    bar = atomic_cas_uint((uint_t *)&bar, (uint_t)foo, 15);
    if (bar)
      return -1;
    if (atomic_add_64_nv((volatile uint64_t *)&foo64, bar64) || foo64)
      return -1;
    bar64 = atomic_swap_64((volatile uint64_t *)&foo64, (uint64_t)bar64);
    if (bar64 || foo64 != 10)
      return -1;
    bar64 = atomic_cas_64((volatile uint64_t *)&bar64, (uint_t)foo64, 15);
    if (bar64)
      return -1;
    atomic_or_64((volatile uint64_t *)&bar64, 0);
    return 0;
  }
"  HAVE_SOLARIS_ATOMIC)

# This is used for the version_compile_machine variable.
IF(SIZEOF_VOIDP MATCHES 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()


MACRO(DIRNAME IN OUT)
  GET_FILENAME_COMPONENT(${OUT} ${IN} PATH)
ENDMACRO()

MACRO(FIND_REAL_LIBRARY SOFTLINK_NAME REALNAME)
  # We re-distribute libstdc++.so which is a symlink.
  # There is no 'readlink' on solaris, so we use perl to follow links:
  SET(PERLSCRIPT
    "my $link= $ARGV[0]; use Cwd qw(abs_path); my $file = abs_path($link); print $file;")
  EXECUTE_PROCESS(
    COMMAND perl -e "${PERLSCRIPT}" ${SOFTLINK_NAME}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE real_library
    )
  SET(REALNAME ${real_library})
ENDMACRO()

MACRO(EXTEND_CXX_LINK_FLAGS LIBRARY_PATH)
  # Using the $ORIGIN token with the -R option to locate the libraries
  # on a path relative to the executable:
  # We need an extra backslash to pass $ORIGIN to the mysql_config script...
  SET(QUOTED_CMAKE_CXX_LINK_FLAGS
    "${CMAKE_CXX_LINK_FLAGS} -R'\\$ORIGIN/../lib' -R${LIBRARY_PATH} ")
  SET(CMAKE_CXX_LINK_FLAGS
    "${CMAKE_CXX_LINK_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
  MESSAGE(STATUS "CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS}")
ENDMACRO()

MACRO(EXTEND_C_LINK_FLAGS LIBRARY_PATH)
  SET(CMAKE_C_LINK_FLAGS
    "${CMAKE_C_LINK_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
  MESSAGE(STATUS "CMAKE_C_LINK_FLAGS ${CMAKE_C_LINK_FLAGS}")
  SET(CMAKE_SHARED_LIBRARY_C_FLAGS
    "${CMAKE_SHARED_LIBRARY_C_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
ENDMACRO()

# We assume that the client code is built with -std=c++11
# Both compilers will use libstdc++.so but possibly different version.
# Hence we install the gcc version here, for use by the server and plugins.
# Install only if INSTALL_GPP_LIBRARIES is set, use for non-native compilers.
IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND CMAKE_COMPILER_IS_GNUCC)
  DIRNAME(${CMAKE_CXX_COMPILER} CXX_PATH)
  SET(LIB_SUFFIX "lib")
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
    SET(LIB_SUFFIX "lib/sparcv9")
  ENDIF()
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    SET(LIB_SUFFIX "lib/amd64")
  ENDIF()
  FIND_LIBRARY(GPP_LIBRARY_NAME
    NAMES "stdc++"
    PATHS ${CXX_PATH}/../${LIB_SUFFIX}
    NO_DEFAULT_PATH
  )
  MESSAGE(STATUS "GPP_LIBRARY_NAME ${GPP_LIBRARY_NAME}")
  IF(GPP_LIBRARY_NAME)
    DIRNAME(${GPP_LIBRARY_NAME} GPP_LIBRARY_PATH)
    FIND_REAL_LIBRARY(${GPP_LIBRARY_NAME} real_library)
    IF(INSTALL_GPP_LIBRARIES)
      MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_NAME} ${real_library}")
      INSTALL(FILES ${GPP_LIBRARY_NAME} ${real_library}
        DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    ENDIF()
    EXTEND_CXX_LINK_FLAGS(${GPP_LIBRARY_PATH})
    EXECUTE_PROCESS(
      COMMAND sh -c "elfdump ${real_library} | grep SONAME"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE sonameline
    )
    IF(NOT result)
      STRING(REGEX MATCH "libstdc.*[^\n]" soname ${sonameline})
      IF(INSTALL_GPP_LIBRARIES)
        MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_PATH}/${soname}")
        INSTALL(FILES "${GPP_LIBRARY_PATH}/${soname}"
          DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
      ENDIF()
    ENDIF()
  ENDIF()
  FIND_LIBRARY(GCC_LIBRARY_NAME
    NAMES "gcc_s"
    PATHS ${CXX_PATH}/../${LIB_SUFFIX}
    NO_DEFAULT_PATH
  )
  IF(GCC_LIBRARY_NAME)
    DIRNAME(${GCC_LIBRARY_NAME} GCC_LIBRARY_PATH)
    FIND_REAL_LIBRARY(${GCC_LIBRARY_NAME} real_library)
    IF(INSTALL_GPP_LIBRARIES)
      MESSAGE(STATUS "INSTALL ${GCC_LIBRARY_NAME} ${real_library}")
      INSTALL(FILES ${GCC_LIBRARY_NAME} ${real_library}
        DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    ENDIF()
    EXTEND_C_LINK_FLAGS(${GCC_LIBRARY_PATH})
  ENDIF()
ENDIF()


# We assume that developer studio runtime libraries are installed.
IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND
   CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  DIRNAME(${CMAKE_CXX_COMPILER} CXX_PATH)

  SET(LIBRARY_SUFFIX "lib/compilers/CC-gcc/lib")
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/sparcv9")
  ENDIF()
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/amd64")
  ENDIF()
  FIND_LIBRARY(STL_LIBRARY_NAME
    NAMES "stdc++"
    PATHS ${CXX_PATH}/../${LIBRARY_SUFFIX}
    NO_DEFAULT_PATH
  )
  MESSAGE(STATUS "STL_LIBRARY_NAME ${STL_LIBRARY_NAME}")
  IF(STL_LIBRARY_NAME)
    DIRNAME(${STL_LIBRARY_NAME} STL_LIBRARY_PATH)
    SET(QUOTED_CMAKE_CXX_LINK_FLAGS
      "${CMAKE_CXX_LINK_FLAGS} -L${STL_LIBRARY_PATH} -R${STL_LIBRARY_PATH}")
    SET(CMAKE_CXX_LINK_FLAGS
      "${CMAKE_CXX_LINK_FLAGS} -L${STL_LIBRARY_PATH} -R${STL_LIBRARY_PATH}")
    SET(CMAKE_C_LINK_FLAGS
      "${CMAKE_C_LINK_FLAGS} -L${STL_LIBRARY_PATH} -R${STL_LIBRARY_PATH}")
  ENDIF()

  SET(CMAKE_C_LINK_FLAGS
    "${CMAKE_C_LINK_FLAGS} -lc")
  SET(CMAKE_CXX_LINK_FLAGS
    "${CMAKE_CXX_LINK_FLAGS} -lstdc++ -lgcc_s -lCrunG3 -lc")
  SET(QUOTED_CMAKE_CXX_LINK_FLAGS
    "${QUOTED_CMAKE_CXX_LINK_FLAGS} -lstdc++ -lgcc_s -lCrunG3 -lc ")

  SET(LIBRARY_SUFFIX "lib/compilers/atomic")
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/sparcv9")
  ENDIF()
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/amd64")
  ENDIF()
  FIND_LIBRARY(ATOMIC_LIBRARY_NAME
    NAMES "statomic"
    PATHS ${CXX_PATH}/../${LIBRARY_SUFFIX}
    NO_DEFAULT_PATH
  )
  MESSAGE(STATUS "ATOMIC_LIBRARY_NAME ${ATOMIC_LIBRARY_NAME}")
  IF(ATOMIC_LIBRARY_NAME)
    DIRNAME(${ATOMIC_LIBRARY_NAME} ATOMIC_LIB_PATH)
    SET(QUOTED_CMAKE_CXX_LINK_FLAGS
    "${QUOTED_CMAKE_CXX_LINK_FLAGS} -L${ATOMIC_LIB_PATH} -R${ATOMIC_LIB_PATH}")
    SET(CMAKE_CXX_LINK_FLAGS
      "${CMAKE_CXX_LINK_FLAGS} -L${ATOMIC_LIB_PATH} -R${ATOMIC_LIB_PATH}")
    SET(CMAKE_C_LINK_FLAGS
      "${CMAKE_C_LINK_FLAGS} -L${ATOMIC_LIB_PATH} -R${ATOMIC_LIB_PATH}")
  ENDIF()

  SET(QUOTED_CMAKE_CXX_LINK_FLAGS
    "${QUOTED_CMAKE_CXX_LINK_FLAGS} -lstatomic ")
ENDIF()

