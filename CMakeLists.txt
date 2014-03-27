# Copyright (c) 2006, 2014, Oracle and/or its affiliates.
# Copyright (c) 2008, 2014, Monty Program Ab
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

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
# Avoid warnings in higher versions
if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" GREATER 2.6)
 CMAKE_POLICY(VERSION 2.8)
endif()

# explicitly set the policy to OLD
# (cannot use NEW, not everyone is on cmake-2.8.12 yet)
IF(POLICY CMP0022)
  CMAKE_POLICY(SET CMP0022 OLD)
ENDIF()

MESSAGE(STATUS "Running cmake version ${CMAKE_VERSION}")

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Distinguish between community and non-community builds, with the
# default being a community build. This does not impact the feature
# set that will be compiled in; it's merely provided as a hint to
# custom packaging steps.
OPTION(COMMUNITY_BUILD "Set to true if this is a community build" ON) 

# Use a default manufacturer if no manufacturer was identified.
IF(NOT DEFINED MANUFACTURER) 
  SET(MANUFACTURER "Built from Source" CACHE  STRING
     "Set the entity that appears as the manufacturer of packages that support a manufacturer field.")
  MARK_AS_ADVANCED(MANUFACTURER)
ENDIF()

SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
   "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel")

IF(UNIX AND NOT APPLE)
  # Note, that generally one should not change settings depending
  # on CMAKE_BUILD_TYPE, because VS and Xcode configure once (with
  # the empty CMAKE_BUILD_TYPE) and the build many times for
  # different build types without re-running cmake!
  # But we only care about WITH_PIC on Unix, where the check for
  # CMAKE_BUILD_TYPE hapen to work.
  IF (CMAKE_BUILD_TYPE MATCHES "Debug")
    SET(WITH_PIC_DEFAULT ON)
  ELSE()
    SET(WITH_PIC_DEFAULT OFF)
  ENDIF()
  # Compiling with PIC speeds up embedded build, on PIC sensitive systems 
  # Predefine it to OFF in release builds, because of the performance penalty
  SET(WITH_PIC ${WITH_PIC_DEFAULT} CACHE BOOL "Compile with PIC.")
ENDIF()

# Optionally set project name, e.g.
# foo.xcodeproj (mac) or foo.sln (windows)
SET(MYSQL_PROJECT_NAME_DOCSTRING "MySQL project name")
IF(DEFINED MYSQL_PROJECT_NAME)
  SET(MYSQL_PROJECT_NAME ${MYSQL_PROJECT_NAME} CACHE STRING
      ${MYSQL_PROJECT_NAME_DOCSTRING} FORCE)
ELSE()
  SET(MYSQL_PROJECT_NAME "MySQL" CACHE STRING
      ${MYSQL_PROJECT_NAME_DOCSTRING} FORCE)
  MARK_AS_ADVANCED(MYSQL_PROJECT_NAME)
ENDIF()
PROJECT(${MYSQL_PROJECT_NAME})

IF(BUILD_CONFIG)
  INCLUDE(
  ${CMAKE_SOURCE_DIR}/cmake/build_configurations/${BUILD_CONFIG}.cmake)
ENDIF()

# Include the platform-specific file. To allow exceptions, this code
# looks for files in order of how specific they are. If there is, for
# example, a generic Linux.cmake and a version-specific
# Linux-2.6.28-11-generic, it will pick Linux-2.6.28-11-generic and
# include it. It is then up to the file writer to include the generic
# version if necessary.
FOREACH(_base
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}
    ${CMAKE_SYSTEM_NAME})
  SET(_file ${CMAKE_SOURCE_DIR}/cmake/os/${_base}.cmake)
  IF(EXISTS ${_file})
    INCLUDE(${_file})
    BREAK()
  ENDIF()
ENDFOREACH()



# Following autotools tradition, add preprocessor definitions
# specified in environment variable CPPFLAGS
IF(DEFINED ENV{CPPFLAGS})
  ADD_DEFINITIONS($ENV{CPPFLAGS})
ENDIF()

#
# Control aspects of the development environment which are
# specific to MySQL maintainers and developers.
#
INCLUDE(maintainer)

SET(MYSQL_MAINTAINER_MODE "AUTO" CACHE STRING "MySQL maintainer-specific development environment. Options are: ON OFF AUTO.")
MARK_AS_ADVANCED(MYSQL_MAINTAINER_MODE)

# Whether the maintainer mode compiler options should be enabled.
IF(CMAKE_C_COMPILER_ID MATCHES "GNU")
  SET_MYSQL_MAINTAINER_GNU_C_OPTIONS()
ENDIF()
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  SET_MYSQL_MAINTAINER_GNU_CXX_OPTIONS()
ENDIF()
IF(CMAKE_C_COMPILER_ID MATCHES "Intel")
  SET_MYSQL_MAINTAINER_INTEL_C_OPTIONS()
ENDIF()
IF(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  SET_MYSQL_MAINTAINER_INTEL_CXX_OPTIONS()
ENDIF()

# Packaging
IF (NOT CPACK_GENERATOR)
  IF(WIN32)
    SET(CPACK_GENERATOR "ZIP")
  ELSE()
    SET(CPACK_GENERATOR "TGZ")
  ENDIF(WIN32) 
ENDIF(NOT CPACK_GENERATOR)

INCLUDE(mysql_version)
INCLUDE(cpack_source_ignore_files)
INCLUDE(install_layout)
INCLUDE(cpack_rpm)
INCLUDE(cpack_deb)

# Add macros
INCLUDE(character_sets)
INCLUDE(zlib)
INCLUDE(ssl)
INCLUDE(readline)
INCLUDE(libutils)
INCLUDE(dtrace)
INCLUDE(jemalloc)
INCLUDE(ctest)
INCLUDE(plugin)
INCLUDE(install_macros)
INCLUDE(mysql_add_executable)

# Handle options
OPTION(DISABLE_SHARED 
 "Don't build shared libraries, compile code as position-dependent" OFF)
IF(DISABLE_SHARED)
  SET(WITHOUT_DYNAMIC_PLUGINS 1)
ENDIF()
OPTION(ENABLED_PROFILING "Enable profiling" ON)
OPTION(CYBOZU "" OFF)
OPTION(BACKUP_TEST "" OFF)
OPTION(WITHOUT_SERVER OFF)
IF(UNIX)
  OPTION(WITH_VALGRIND "Valgrind instrumentation" OFF)
ENDIF()
OPTION (WITH_UNIT_TESTS "Compile MySQL with unit tests" ON)
MARK_AS_ADVANCED(CYBOZU BACKUP_TEST WITHOUT_SERVER DISABLE_SHARED)

OPTION(NOT_FOR_DISTRIBUTION "Allow linking with GPLv2-incompatible system libraries. Only set it you never plan to distribute the resulting binaries" OFF)
 
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
# We need some extra FAIL_REGEX patterns
# Note that CHECK_C_SOURCE_COMPILES is a misnomer, it will also link.
MACRO (MY_CHECK_C_COMPILER_FLAG FLAG RESULT)
  SET(SAVE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${FLAG}")
  CHECK_C_SOURCE_COMPILES("int main(void) { return 0; }" ${RESULT}
    FAIL_REGEX "argument unused during compilation"
    FAIL_REGEX "unsupported .*option"
    FAIL_REGEX "unknown .*option"
    FAIL_REGEX "unrecognized .*option"
    FAIL_REGEX "ignoring unknown option"
    FAIL_REGEX "[Ww]arning: [Oo]ption"
    )
  SET(CMAKE_REQUIRED_FLAGS "${SAVE_CMAKE_REQUIRED_FLAGS}")
ENDMACRO()

MACRO (MY_CHECK_CXX_COMPILER_FLAG FLAG RESULT)
  SET(SAVE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${FLAG}")
  CHECK_CXX_SOURCE_COMPILES("int main(void) { return 0; }" ${RESULT}
    FAIL_REGEX "argument unused during compilation"
    FAIL_REGEX "unsupported .*option"
    FAIL_REGEX "unknown .*option"
    FAIL_REGEX "unrecognized .*option"
    FAIL_REGEX "ignoring unknown option"
    FAIL_REGEX "[Ww]arning: [Oo]ption"
    )
  SET(CMAKE_REQUIRED_FLAGS "${SAVE_CMAKE_REQUIRED_FLAGS}")
ENDMACRO()

OPTION(WITH_ASAN "Enable address sanitizer" OFF)
IF (WITH_ASAN)
  # gcc 4.8.1 and new versions of clang
  MY_CHECK_C_COMPILER_FLAG("-fsanitize=address" HAVE_C_FSANITIZE)
  MY_CHECK_CXX_COMPILER_FLAG("-fsanitize=address" HAVE_CXX_FSANITIZE)

  IF(HAVE_C_FSANITIZE AND HAVE_CXX_FSANITIZE)
    # We switch on basic optimization also for debug builds.
    # With optimization we may get some warnings, so we switch off -Werror
    SET(CMAKE_C_FLAGS_DEBUG
      "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -O1 -Wno-error -fPIC")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO
      "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address -fPIC")
    SET(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -O1 -Wno-error -fPIC")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address -fPIC")
    SET(WITH_ASAN_OK 1)
  ELSE()
    # older versions of clang
    MY_CHECK_C_COMPILER_FLAG("-faddress-sanitizer" HAVE_C_FADDRESS)
    MY_CHECK_CXX_COMPILER_FLAG("-faddress-sanitizer" HAVE_CXX_FFADDRESS)

    IF(HAVE_C_FADDRESS AND HAVE_CXX_FFADDRESS)
      # We switch on basic optimization also for debug builds.
      SET(CMAKE_C_FLAGS_DEBUG
        "${CMAKE_C_FLAGS_DEBUG} -faddress-sanitizer -O1 -fPIC")
      SET(CMAKE_C_FLAGS_RELWITHDEBINFO
        "${CMAKE_C_FLAGS_RELWITHDEBINFO} -faddress-sanitizer -fPIC")
      SET(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -faddress-sanitizer -O1 -fPIC")
      SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
        "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -faddress-sanitizer -fPIC")
      SET(WITH_ASAN_OK 1)
    ENDIF()
  ENDIF()

  IF(NOT WITH_ASAN_OK)
    MESSAGE(FATAL_ERROR "Do not know how to enable address sanitizer")
  ENDIF()
ENDIF()


OPTION(ENABLE_DEBUG_SYNC "Enable debug sync (debug builds only)" ON) 
IF(ENABLE_DEBUG_SYNC) 
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLED_DEBUG_SYNC") 
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DENABLED_DEBUG_SYNC") 
ENDIF() 
 
OPTION(ENABLE_GCOV "Enable gcov (debug, Linux builds only)" OFF)
IF (ENABLE_GCOV AND NOT WIN32 AND NOT APPLE)
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
  SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -lgcov")
ENDIF()

OPTION(ENABLED_LOCAL_INFILE
 "If we should should enable LOAD DATA LOCAL by default" ${IF_WIN})
MARK_AS_ADVANCED(ENABLED_LOCAL_INFILE)

OPTION(WITH_FAST_MUTEXES "Compile with fast mutexes" OFF)
MARK_AS_ADVANCED(WITH_FAST_MUTEXES)

# Set DBUG_OFF and other optional release-only flags for non-debug project types
FOREACH(BUILD_TYPE RELEASE RELWITHDEBINFO MINSIZEREL)
  FOREACH(LANG C CXX)
    IF (NOT CMAKE_${LANG}_FLAGS_${BUILD_TYPE} MATCHES "DDBUG_" AND
        NOT CMAKE_${LANG}_FLAGS MATCHES "DDBUG_")
      SET(CMAKE_${LANG}_FLAGS_${BUILD_TYPE} 
       "${CMAKE_${LANG}_FLAGS_${BUILD_TYPE}} -DDBUG_OFF")
    ENDIF()
    IF(WITH_FAST_MUTEXES)
      SET(CMAKE_${LANG}_FLAGS_${BUILD_TYPE} 
        "${CMAKE_${LANG}_FLAGS_${BUILD_TYPE}} -DMY_PTHREAD_FASTMUTEX=1")
    ENDIF()
  ENDFOREACH()
ENDFOREACH()

# Add safemutex for debug configurations, except on Windows
# (safemutex has never worked on Windows)
IF(NOT WIN32)
  FOREACH(LANG C CXX)
      SET(CMAKE_${LANG}_FLAGS_DEBUG "${CMAKE_${LANG}_FLAGS_DEBUG} -DSAFE_MUTEX")
  ENDFOREACH()
ENDIF()

# safemalloc can be enabled and disabled independently
SET(WITH_SAFEMALLOC "AUTO" CACHE STRING "Use safemalloc memory debugger. Will result in slower execution. Options are: ON OFF AUTO.")

# force -DUSE_MYSYS_NEW unless already done by HAVE_CXX_NEW
IF(HAVE_CXX_NEW)
  SET(DUSE_MYSYS_NEW "-DUSE_MYSYS_NEW")
ENDIF()

IF(WITH_SAFEMALLOC MATCHES "ON")
  ADD_DEFINITIONS( -DSAFEMALLOC ${DUSE_MYSYS_NEW})
ELSEIF(WITH_SAFEMALLOC MATCHES "AUTO" AND NOT WIN32 AND NOT WITH_VALGRIND)
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC ${DUSE_MYSYS_NEW}")
ENDIF()

# Set commonly used variables
IF(WIN32)
  SET(DEFAULT_MYSQL_HOME "C:/MariaDB${MYSQL_BASE_VERSION}")
  SET(SHAREDIR share)
ELSE()
  SET(DEFAULT_MYSQL_HOME ${CMAKE_INSTALL_PREFIX})
  SET(SHAREDIR ${DEFAULT_MYSQL_HOME}/${INSTALL_MYSQLSHAREDIR})
ENDIF()

SET(DEFAULT_BASEDIR "${DEFAULT_MYSQL_HOME}")
IF(INSTALL_MYSQLDATADIR MATCHES "^/.*")
  SET(MYSQL_DATADIR ${INSTALL_MYSQLDATADIR} CACHE PATH "default MySQL data directory")
ELSE()
  SET(MYSQL_DATADIR "${DEFAULT_MYSQL_HOME}/${INSTALL_MYSQLDATADIR}" CACHE PATH "default MySQL data directory")
ENDIF()
SET(DEFAULT_CHARSET_HOME "${DEFAULT_MYSQL_HOME}")
SET(PLUGINDIR "${DEFAULT_MYSQL_HOME}/${INSTALL_PLUGINDIR}")
IF(INSTALL_SYSCONFDIR)
  SET(DEFAULT_SYSCONFDIR "${INSTALL_SYSCONFDIR}")
ENDIF()

SET(TMPDIR ""
  CACHE PATH
  "PATH to MySQL TMP dir. Defaults to the P_tmpdir macro in <stdio.h>")
IF(TMPDIR STREQUAL "")
  # Do not quote it, to refer to the P_tmpdir macro.
  SET(DEFAULT_TMPDIR "P_tmpdir")
ELSE()
  # Quote it, to make it a const char string.
  SET(DEFAULT_TMPDIR "\"${TMPDIR}\"")
ENDIF()

# Run platform tests
INCLUDE(configure.cmake)

# Common defines and includes
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/include)

# Add bundled or system zlib.
MYSQL_CHECK_ZLIB_WITH_COMPRESS()
# Optionally add bundled yassl/taocrypt or system openssl.
MYSQL_CHECK_SSL()
# Add readline or libedit.
MYSQL_CHECK_READLINE()

CHECK_JEMALLOC()

#
# Setup maintainer mode options by the end. Platform checks are
# not run with the warning options as to not perturb fragile checks
# (i.e. do not make warnings into errors).
#
IF(MYSQL_MAINTAINER_MODE MATCHES "ON")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_MAINTAINER_C_WARNINGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_MAINTAINER_CXX_WARNINGS}")
ELSEIF(MYSQL_MAINTAINER_MODE MATCHES "AUTO")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_MAINTAINER_C_WARNINGS}")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${MY_MAINTAINER_CXX_WARNINGS}")
ENDIF()

IF(WITH_UNIT_TESTS)
 ENABLE_TESTING()
 ADD_SUBDIRECTORY(unittest/mytap)
 ADD_SUBDIRECTORY(unittest/strings)
 ADD_SUBDIRECTORY(unittest/examples)
 ADD_SUBDIRECTORY(unittest/mysys)
 ADD_SUBDIRECTORY(unittest/my_decimal)
ENDIF()

IF(NOT WITHOUT_SERVER)
SET (MYSQLD_STATIC_PLUGIN_LIBS "" CACHE INTERNAL "")
 # Add storage engines and plugins.
 CONFIGURE_PLUGINS()
ENDIF()

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(dbug)
ADD_SUBDIRECTORY(strings)
ADD_SUBDIRECTORY(vio)
ADD_SUBDIRECTORY(regex)
ADD_SUBDIRECTORY(mysys)
ADD_SUBDIRECTORY(libmysql)
ADD_SUBDIRECTORY(client)
ADD_SUBDIRECTORY(extra)
ADD_SUBDIRECTORY(libservices)
ADD_SUBDIRECTORY(scripts)
ADD_SUBDIRECTORY(sql/share)
ADD_SUBDIRECTORY(support-files)

IF(NOT WITHOUT_SERVER)
  ADD_SUBDIRECTORY(tests)
  ADD_SUBDIRECTORY(sql)
  OPTION (WITH_EMBEDDED_SERVER "Compile MySQL with embedded server" OFF)
  IF(WITH_EMBEDDED_SERVER) 
   ADD_SUBDIRECTORY(libmysqld)
   ADD_SUBDIRECTORY(libmysqld/examples)
  ENDIF(WITH_EMBEDDED_SERVER)

  ADD_SUBDIRECTORY(mysql-test)
  ADD_SUBDIRECTORY(mysql-test/lib/My/SafeProcess)
  ADD_SUBDIRECTORY(sql-bench)

  IF(EXISTS ${CMAKE_SOURCE_DIR}/internal/CMakeLists.txt)
    ADD_SUBDIRECTORY(internal)
  ENDIF()
  ADD_SUBDIRECTORY(packaging/rpm-uln)
  ADD_SUBDIRECTORY(packaging/rpm-oel)
ENDIF()

IF(UNIX)
  ADD_SUBDIRECTORY(man)
ENDIF()

INCLUDE(cmake/abi_check.cmake)
INCLUDE(cmake/tags.cmake)

IF(WIN32)
  ADD_SUBDIRECTORY(win/upgrade_wizard)
  ADD_SUBDIRECTORY(win/packaging)
ENDIF()
ADD_SUBDIRECTORY(packaging/solaris)

CONFIGURE_FILE(config.h.cmake   ${CMAKE_BINARY_DIR}/include/my_config.h)
CONFIGURE_FILE(config.h.cmake   ${CMAKE_BINARY_DIR}/include/config.h)
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/include/mysql_version.h.in
               ${CMAKE_BINARY_DIR}/include/mysql_version.h )
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/sql/sql_builtin.cc.in
    ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc)
CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/cmake/info_macros.cmake.in ${CMAKE_BINARY_DIR}/info_macros.cmake @ONLY)

IF(DEB)
  CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/debian/mariadb-server-5.5.files.in
    ${CMAKE_SOURCE_DIR}/debian/mariadb-server-5.5.files)
ENDIF(DEB)

# Handle the "INFO_*" files.
INCLUDE(${CMAKE_BINARY_DIR}/info_macros.cmake)
# Source: This can be done during the cmake phase, all information is
# available, but should be repeated on each "make" just in case someone
# does "cmake ; make ; bzr pull ; make".
CREATE_INFO_SRC(${CMAKE_BINARY_DIR}/Docs)
ADD_CUSTOM_TARGET(INFO_SRC ALL
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/info_src.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# Build flags: This must be postponed to the make phase.
ADD_CUSTOM_TARGET(INFO_BIN ALL
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/info_bin.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

INSTALL_DOCUMENTATION(README COPYING COPYING.LESSER
                      COMPONENT Readme)
INSTALL_DOCUMENTATION(${CMAKE_BINARY_DIR}/Docs/INFO_SRC
                      ${CMAKE_BINARY_DIR}/Docs/INFO_BIN)
IF(UNIX)
  INSTALL_DOCUMENTATION(Docs/INSTALL-BINARY COMPONENT Readme)
ENDIF()

INCLUDE(CPack)

IF(NON_DISTRIBUTABLE_WARNING)
  MESSAGE(WARNING "
You have linked MariaDB with GPLv3 libraries!  You may not distribute the resulting binary. If you do, you will put yourself into a legal problem with Free Software Foundation.")
ENDIF()

