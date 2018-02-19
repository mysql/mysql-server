# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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

# Disable specific types of warnings for current directory
# if the compiler supports the flag
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  INCLUDE(CheckCXXCompilerFlag)
  FOREACH(warning
          "unused-but-set-variable"
          "strict-aliasing"
          "unused-parameter"
          # Downgrade -Werror to warning for "may be used uninitialized"
          # and "is used uninitialized in this function"
          "error=maybe-uninitialized")
    STRING(REPLACE "-" "_" warning_ ${warning})
    STRING(TOUPPER ${warning_} WARNING)
    CHECK_CXX_COMPILER_FLAG("-W${warning}" HAVE_${WARNING})
    IF(HAVE_${WARNING})
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warning}")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${warning}")
    ENDIF()
  ENDFOREACH()
ENDIF()

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitools.cmake)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/bison.cmake)
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel/signaldata
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbtools
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
  ${CMAKE_BINARY_DIR}/storage/ndb/tools)

ADD_CONVENIENCE_LIBRARY(ndbtools
  NdbImport.cpp
  NdbImportImpl.cpp
  NdbImportCsv.cpp
  NdbImportCsvGram.cpp
  NdbImportUtil.cpp)

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Suppress warnings for undefined identifiers in bison output
  ADD_COMPILE_FLAGS(${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
                    COMPILE_FLAGS "-Wno-undef")
ENDIF()

COPY_BISON_OUTPUT(
  ${CMAKE_CURRENT_SOURCE_DIR}/NdbImportCsvGram.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/NdbImportCsvGram.hpp
  ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.hpp
)

RUN_BISON(
  ${CMAKE_CURRENT_SOURCE_DIR}/NdbImportCsvGram.ypp
  ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/NdbImportCsvGram.hpp
  NdbImportCsv_yy
)

MYSQL_ADD_EXECUTABLE(ndb_waiter
  waiter.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_waiter ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_table
  drop_tab.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_table ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_delete_all
  delete_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_delete_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_desc
  desc.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_desc ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_index
  drop_index.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_index ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_show_tables
  listTables.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_show_tables ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_all
  select_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_count
  select_count.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_count ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_perror
  ndb_perror.cpp
  ${NDB_SOURCE_DIR}/src/mgmapi/mgmapi_error.cpp
  ${NDB_SOURCE_DIR}/src/kernel/error/ndbd_exit_codes.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_perror ndbclient_static)

MYSQL_ADD_EXECUTABLE(ndb_restore
  restore/restore_main.cpp
  restore/consumer.cpp
  restore/consumer_restore.cpp
  restore/consumer_printer.cpp
  restore/Restore.cpp
  ndb_lib_move_data.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_restore ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_import
  ndb_import.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_import ndbtools ndbclient_static)

NDB_ADD_TEST(NdbImportUtil-t NdbImportUtil.cpp
             LIBS ndbtools ndbclient_static)

NDB_ADD_TEST(NdbImportCsv-t NdbImportCsv.cpp
             LIBS ndbtools ndbclient_static)

MYSQL_ADD_EXECUTABLE(ndb_config
  ndb_config.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_config ndbclient_static)

# Build ndbinfo_sql and run it to create ndbinfo.sql
MYSQL_ADD_EXECUTABLE(ndbinfo_sql
  ndbinfo_sql.cpp
  SKIP_INSTALL
)
TARGET_LINK_LIBRARIES(ndbinfo_sql ndbclient_static)

ADD_CUSTOM_COMMAND(TARGET ndbinfo_sql POST_BUILD
  COMMAND $<TARGET_FILE:ndbinfo_sql>
  > ${PROJECT_BINARY_DIR}/storage/ndb/tools/ndbinfo.sql
  )

MYSQL_ADD_EXECUTABLE(ndb_index_stat
  ndb_index_stat.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_index_stat ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndbinfo_select_all
  ndbinfo_select_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndbinfo_select_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_blob_tool
  ndb_blob_tool.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_blob_tool ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_move_data
  ndb_move_data.cpp
  ndb_lib_move_data.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_move_data ndbclient_static ndbNDBT)

IF(NOT WIN32)
IF(NOT CMAKE_SYSTEM_NAME MATCHES "SunOS")
  IF (HAVE_NCURSESW_1 OR HAVE_NCURSESW_2 OR HAVE_NCURSESW_3 OR HAVE_NCURSESW_4 OR HAVE_NCURSESW_5)
    MESSAGE(STATUS "-- Adding ndb_top executable")
    FIND_LIBRARY(NCURSESW_LIB
                 NAMES ncursesw)
    IF (NOT NCURSESW_LIB)
      FIND_LIBRARY(NCURSESW_LIB
                   NAMES ncurses)
    ENDIF()

    IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      # Suppress warnings for undefined identifiers in ndb_top
      ADD_COMPILE_FLAGS(${CMAKE_CURRENT_SOURCE_DIR}/ndb_top.cpp
                        COMPILE_FLAGS "-Wno-undef")
    ENDIF()

    CHECK_LIBRARY_EXISTS("${NCURSESW_LIB}"
      stdscr "" NCURSES_HAS_STDSCR)
    IF(NOT NCURSES_HAS_STDSCR)
      FIND_LIBRARY(NCURSES_TINFO_LIB
                   NAMES tinfo)
    ENDIF()
    MYSQL_ADD_EXECUTABLE(ndb_top
      ndb_top.cpp
      COMPONENT ClusterTools)
    IF(NCURSES_HAS_STDSCR)
      TARGET_LINK_LIBRARIES(ndb_top mysqlclient ${NCURSESW_LIB})
    ELSE()
      TARGET_LINK_LIBRARIES(ndb_top
                            mysqlclient
                            ${NCURSESW_LIB}
                            ${NCURSES_TINFO_LIB})
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "ndb_top executable could not be built, install ncursesw library")
  ENDIF()
ENDIF()
ENDIF()

# Install the ndb_dist_priv.sql script
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ndb_dist_priv.sql
        DESTINATION ${INSTALL_MYSQLSHAREDIR})

if(NOT WIN32)
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
        DESTINATION ${INSTALL_BINDIR})
