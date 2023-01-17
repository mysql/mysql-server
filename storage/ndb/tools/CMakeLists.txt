# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

INCLUDE(ndb_downgrade_warnings)

# Disable specific types of warnings for current directory
# if the compiler supports the flag
FOREACH(warning
    "cast-qual"
    "maybe-uninitialized"
    "unused-but-set-variable"
    "unused-parameter"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
    STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

# Check BISON_VERSION and set BISON_FLAGS_WARNINGS
FIND_PACKAGE(BISON REQUIRED)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel/signaldata
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
  ${CMAKE_SOURCE_DIR}/storage/ndb/test/include
  ${CMAKE_BINARY_DIR}/storage/ndb/tools)

ADD_LIBRARY(ndbtools STATIC
  NdbImport.cpp
  NdbImportImpl.cpp
  NdbImportCsv.cpp
  NdbImportCsvGram.cpp
  NdbImportUtil.cpp)

ADD_DEPENDENCIES(clang_tidy_prerequisites ndbtools)

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  # Suppress warnings for undefined identifiers in bison output
  # For implicit-fallthrough - see Bug#34098818
  ADD_COMPILE_FLAGS(${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
    COMPILE_FLAGS "-Wno-undef -Wno-implicit-fallthrough")
ENDIF()

IF(MSVC)
  # Suppress warnings for switch statement with no 'case' labels
  ADD_COMPILE_FLAGS(${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
    COMPILE_FLAGS "/wd4065")
ENDIF()

BISON_TARGET(ndb_import
  ${CMAKE_CURRENT_SOURCE_DIR}/NdbImportCsvGram.ypp
  ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
  COMPILE_FLAGS
  "--name-prefix=NdbImportCsv_yy --yacc ${BISON_FLAGS_WARNINGS} ${BISON_NO_LINE_OPT}"
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.hpp
  )

# Add target, for easier debugging.
# The new Xcode build system does not support multiple targets that
# DEPEND on the same outputs.
IF(NOT APPLE_XCODE)
  MY_ADD_CUSTOM_TARGET(GenBison_ndb_import DEPENDS ${BISON_ndb_import_OUTPUTS})
ENDIF()

OPTION(NDB_UTILS_LINK_DYNAMIC
  "Dynamically link NDB utilities link with shared ndbclient" OFF)

IF(NDB_UTILS_LINK_DYNAMIC)
  SET(LINK_NDBCLIENT NDBCLIENT)
ELSE()
  SET(LINK_NDBCLIENT STATIC_NDBCLIENT)
ENDIF()

NDB_ADD_EXECUTABLE(ndb_waiter
  waiter.cpp
  ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_drop_table
  drop_tab.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_delete_all
  delete_all.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_desc
  desc.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_drop_index
  drop_index.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_show_tables
  listTables.cpp
  ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_select_all
  select_all.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_select_count
  select_count.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_perror
  ndb_perror.cpp
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi/mgmapi_error.cpp
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi/ndbd_exit_codes.cpp
  COMPONENT ClusterTools
  ${LINK_NDBCLIENT}
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_restore
  restore/restore_main.cpp
  restore/consumer.cpp
  restore/consumer_restore.cpp
  restore/consumer_printer.cpp
  restore/Restore.cpp
  ndb_lib_move_data.cpp
  ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  LINK_LIBRARIES ndbgeneral ndbportlib
  )
NDB_ADD_EXECUTABLE(ndb_import
  ndb_import.cpp
  ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  LINK_LIBRARIES ndbtools ndbgeneral
  )

NDB_ADD_TEST(NdbImportUtil-t NdbImportUtil.cpp
  LIBS ndbtools ndbclient_static)

NDB_ADD_TEST(NdbImportCsv-t NdbImportCsv.cpp
  LIBS ndbtools ndbclient_static)

NDB_ADD_EXECUTABLE(ndb_config
  ndb_config.cpp
  ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )

NDB_ADD_EXECUTABLE(ndb_index_stat
  ndb_index_stat.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndbinfo_select_all
  ndbinfo_select_all.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )
NDB_ADD_EXECUTABLE(ndb_blob_tool
  ndb_blob_tool.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )

NDB_ADD_EXECUTABLE(ndb_move_data
  ndb_move_data.cpp
  ndb_lib_move_data.cpp
  NDBTEST ${LINK_NDBCLIENT}
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  )

NDB_ADD_EXECUTABLE(ndbxfrm
        ndbxfrm.cpp
        COMPONENT ClusterTools
        ENABLE_EXPORTS
        LINK_LIBRARIES ndbmgmapi ndbgeneral ndbportlib
        )

NDB_ADD_EXECUTABLE(ndb_secretsfile_reader
        ndb_secretsfile_reader.cpp
        COMPONENT ClusterTools
        ENABLE_EXPORTS
        LINK_LIBRARIES ndbmgmapi ndbgeneral ndbportlib
        )

IF(NOT WIN32)
  IF(HAVE_NCURSESW_CURSES_H OR
     HAVE_NCURSESW_H OR
     HAVE_NCURSES_H OR
     HAVE_NCURSES_CURSES_H)

    MESSAGE(STATUS "-- Adding ndb_top executable")
    FIND_LIBRARY(NCURSESW_LIB
      NAMES ncursesw)
    FIND_LIBRARY(NCURSESW_LIB
      NAMES ncurses)

    CHECK_LIBRARY_EXISTS("${NCURSESW_LIB}"
      stdscr "" NCURSES_HAS_STDSCR)
    IF(NOT NCURSES_HAS_STDSCR)
      FIND_LIBRARY(NCURSES_TINFO_LIB NAMES tinfo)
    ENDIF()
    MYSQL_ADD_EXECUTABLE(ndb_top
      ndb_top.cpp
      COMPONENT ClusterTools
      )
    IF(NCURSES_HAS_STDSCR)
      TARGET_LINK_LIBRARIES(ndb_top mysqlclient ${NCURSESW_LIB})
    ELSE()
      TARGET_LINK_LIBRARIES(ndb_top
        mysqlclient
        ${NCURSESW_LIB}
        ${NCURSES_TINFO_LIB})
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR
      "ndb_top executable could not be built, install ncursesw library")
  ENDIF()
ENDIF()

IF(NOT WIN32)
  SET(TARGET_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/runtime_output_directory)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/ndb_error_reporter.pl
    ${TARGET_RUNTIME_OUTPUT_DIRECTORY}/ndb_error_reporter COPYONLY)
  EXECUTE_PROCESS(COMMAND chmod +x
    ${TARGET_RUNTIME_OUTPUT_DIRECTORY}/ndb_error_reporter)
  INSTALL_SCRIPT(${TARGET_RUNTIME_OUTPUT_DIRECTORY}/ndb_error_reporter
    DESTINATION ${INSTALL_BINDIR}
    COMPONENT ClusterTools)
ENDIF()

INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/ndb_size.pl
  DESTINATION ${INSTALL_BINDIR}
  )
