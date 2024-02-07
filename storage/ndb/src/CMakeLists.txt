# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(libutils)

INCLUDE(ndb_downgrade_warnings)

# Disable specific types of warnings for current directory
# if the compiler supports the flag
FOREACH(warning
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
    STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(mgmapi)
ADD_SUBDIRECTORY(ndbapi)

#
# Build static ndbclient library
#
SET(LIBS_TO_MERGE
  mysys
  mytime
  ndbapi
  ndbgeneral
  ndblogger
  ndbmgmapi
  ndbmgmcommon
  ndbportlib
  ndbsignaldata
  ndbtrace
  ndbtransport
  strings
  )

SET(LIBS_TO_LINK)

IF(WITH_ZLIB STREQUAL "bundled")
  LIST(APPEND LIBS_TO_MERGE zlib)
ELSE()
  LIST(APPEND LIBS_TO_LINK ext::zlib)
ENDIF()

IF(WITH_EXT_BACKTRACE)
  LIST(APPEND LIBS_TO_MERGE backtrace)
ENDIF()

MESSAGE(STATUS "LIBS_TO_MERGE ${LIBS_TO_MERGE}")

IF(NOT WITH_NDB)
  SET(SKIP_INSTALL_NDBCLIENT_STATIC SKIP_INSTALL)
ENDIF()

MERGE_CONVENIENCE_LIBRARIES(ndbclient_static ${LIBS_TO_MERGE}
  ${SKIP_INSTALL_NDBCLIENT_STATIC}
  COMPONENT Development
  LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto ${LIBS_TO_LINK}
  )

IF(NOT WITH_NDB)
  RETURN()
ENDIF()

# Build test program to check linking against ndclient_static
ADD_EXECUTABLE(ndbclient_static_link_test ndbclient_link_test.cpp)
SET_TARGET_PROPERTIES(ndbclient_static_link_test PROPERTIES ENABLE_EXPORTS TRUE)
TARGET_LINK_LIBRARIES(ndbclient_static_link_test ndbclient_static)

IF(NOT WITHOUT_SERVER)
  ADD_SUBDIRECTORY(kernel)
ENDIF()
ADD_SUBDIRECTORY(mgmclient)
ADD_SUBDIRECTORY(mgmsrv)

IF(BUILD_IS_SINGLE_CONFIG)
  SET(JAVA_SUBDIR)
ELSE()
  SET(JAVA_SUBDIR $<CONFIG>)
ENDIF()

IF(WIN32)
  SET(JAVA_SEPARATOR ";")
ELSE()
  set(JAVA_SEPARATOR :)
ENDIF()

SET(NDBJTIE_LIB)
IF(WITH_NDB_JAVA)
  ADD_SUBDIRECTORY(ndbjtie)
  SET(NDBJTIE_LIB ndbjtie)
  ADD_DEFINITIONS(-DNDB_WITH_NDBJTIE)
ENDIF()

#
# Build shared ndbclient library
#
SET(NDBCLIENT_SO_LIBS ${LIBS_TO_MERGE} ${NDBJTIE_LIB})
SET(NDB_SHARED_LIB_VERSION "6.1.0")

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/libndbclient.ver.in
  ${CMAKE_CURRENT_BINARY_DIR}/libndbclient.ver)

# This is needed for java-based unit tests.
IF(MY_COMPILER_IS_CLANG AND WITH_UBSAN)
  LIST(APPEND LIBS_TO_LINK -lubsan)
ENDIF()

# This contains a dummy function _ndbclient_exports() which calls
# enough symbols to pull in everything we need.
# libndbclient.ver will export everything, except mysys functions
# handle_options my_init my_load_defaults
ADD_CONVENIENCE_LIBRARY(ndbclient_exports ndbclient_exports.cpp)

IF(WIN32)
  # We have no explicit list of symbols to EXPORT in a .def file.
  # So instead we create a shared library from ndbclient_exports.cpp.
  # It will pull in, and make visible, all the rest.
  ADD_SHARED_LIBRARY(ndbclient_so ndbclient_exports.cpp
    COMPONENT Development
    DESTINATION "${INSTALL_LIBDIR}"
    LINK_LIBRARIES ${NDBCLIENT_SO_LIBS} ${NDBJTIE_LIB} ${LIBS_TO_LINK}
    OUTPUT_NAME ndbclient
    SOVERSION ${NDB_SHARED_LIB_VERSION}
    )
ELSE()
  # Merge libraries, tag _ndbclient_exports as explicitly needed,
  # and it will transitively pull in the rest.
  MERGE_LIBRARIES_SHARED(ndbclient_so ndbclient_exports ${NDBCLIENT_SO_LIBS}
    EXPORTS _ndbclient_exports
    COMPONENT Development
    DESTINATION "${INSTALL_LIBDIR}"
    LINK_LIBRARIES ${NDBJTIE_LIB} ${LIBS_TO_LINK}
    OUTPUT_NAME ndbclient
    SOVERSION ${NDB_SHARED_LIB_VERSION}
    )
ENDIF()

IF(NOT WIN32)
  SET(NDBCLIENT_SYSTEM_LIBS)
  FOREACH(lib ${NDBCLIENT_SO_LIBS})
    LIST(FIND KNOWN_CONVENIENCE_LIBRARIES ${lib} FOUNDIT)
    IF(FOUNDIT LESS 0)
      MESSAGE(STATUS "Known libs : ${KNOWN_CONVENIENCE_LIBRARIES}")
      MESSAGE(FATAL_ERROR "Unknown static library ${lib} FOUNDIT ${FOUNDIT}")
    ENDIF()
    GET_DEPENDEND_OS_LIBS(${lib} LIB_OSLIBS)
    IF(LIB_OSLIBS)
      LIST(APPEND NDBCLIENT_SYSTEM_LIBS ${LIB_OSLIBS})
    ENDIF()
  ENDFOREACH()

  IF(NDBCLIENT_SYSTEM_LIBS)
    LIST(REMOVE_DUPLICATES NDBCLIENT_SYSTEM_LIBS)
    MESSAGE(STATUS "System libs used by ndbclient_so: ${NDBCLIENT_SYSTEM_LIBS}")
    TARGET_LINK_LIBRARIES(ndbclient_so INTERFACE
      ${NDBCLIENT_SYSTEM_LIBS})
  ENDIF()

  # Prepend any special linker flags(like -m64) for shared library
  GET_TARGET_PROPERTY(ndbclient_so_link_flags ndbclient_so LINK_FLAGS)
  IF(NOT ndbclient_so_link_flags)
    # Avoid LINK_FLAGS-NOTFOUND
    SET(ndbclient_so_link_flags)
  ENDIF()
  SET(ndbclient_so_link_flags
    "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${ndbclient_so_link_flags}")
  IF(LINK_FLAG_NO_UNDEFINED)
    STRING_APPEND(ndbclient_so_link_flags " ${LINK_FLAG_NO_UNDEFINED}")
    IF(LINUX)
      STRING_APPEND(ndbclient_so_link_flags
        " -Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/libndbclient.ver")
    ENDIF()
  ENDIF()
  IF(SOLARIS)
    STRING_APPEND(ndbclient_so_link_flags " ${CMAKE_CXX_LINK_FLAGS}")
  ENDIF()
  SET_TARGET_PROPERTIES(ndbclient_so
    PROPERTIES LINK_FLAGS "${ndbclient_so_link_flags}")
ENDIF()

# Build test program to check linking against ndclient_so
ADD_EXECUTABLE(ndbclient_shared_link_test ndbclient_link_test.cpp)
SET_TARGET_PROPERTIES(ndbclient_shared_link_test PROPERTIES ENABLE_EXPORTS TRUE)
TARGET_LINK_LIBRARIES(ndbclient_shared_link_test ndbclient_so)
