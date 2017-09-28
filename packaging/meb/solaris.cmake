# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All Rights Reversed.

#
# This program is NOT released under the GPL license, but constitutes
# a trade secret of Oracle. Use, publication, and redistribution
# of this program is prohibited without a written permission from
# Oracle.
#
# Most of the code below is from the MySQL Server top "configure.cmake"
#

INCLUDE(CheckCSourceRuns)

SET(INSTALL_LIBDIR "lib")

MACRO(DIRNAME IN OUT)
  GET_FILENAME_COMPONENT(${OUT} ${IN} PATH)
ENDMACRO()

MACRO(FIND_REAL_LIBRARY SOFTLINK_NAME REALNAME)
  # We re-distribute libstlport.so/libstdc++.so which are both symlinks.
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

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_INSTALL_RPATH "$ORIGIN/../${INSTALL_LIBDIR}")
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
    MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_NAME} ${real_library}")
    INSTALL(FILES ${GPP_LIBRARY_NAME} ${real_library}
            DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    EXECUTE_PROCESS(
      COMMAND sh -c "elfdump ${real_library} | grep SONAME"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE sonameline
    )
    IF(NOT result)
      STRING(REGEX MATCH "libstdc.*[^\n]" soname ${sonameline})
      MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_PATH}/${soname}")
      INSTALL(FILES "${GPP_LIBRARY_PATH}/${soname}"
              DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
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
    MESSAGE(STATUS "INSTALL ${GCC_LIBRARY_NAME} ${real_library}")
    INSTALL(FILES ${GCC_LIBRARY_NAME} ${real_library}
            DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
  ENDIF()

  IF(NOT CMAKE_CROSSCOMPILING)
    CHECK_C_SOURCE_RUNS(
    "#include<stdint.h>
    int main()
    {
        unsigned char	a = 0;
        unsigned char	b = 0;
        unsigned char	c = 1;

        __atomic_exchange(&a, &b,  &c, __ATOMIC_RELEASE);
        __atomic_compare_exchange(&a, &b, &c, 0,
                      __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
        return(0);
    }"
    HAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE
    )
  ENDIF()

  IF(HAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE)
    ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE=1)
  ENDIF()
ENDIF()
