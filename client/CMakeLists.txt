# Copyright (c) 2006, 2024, Oracle and/or its affiliates.
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

ADD_WSHADOW_WARNING()

# We are not interesting in profiling clients.
DISABLE_MISSING_PROFILE_WARNING()

## Subdirectory for mysql_migrate_keyring code.
ADD_SUBDIRECTORY(migrate_keyring)

MYSQL_ADD_EXECUTABLE(mysql
  ${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc
  completion_hash.cc
  mysql.cc
  pattern_matcher.cc
  readline.cc
  client_query_attributes.cc
  multi_factor_passwordopt-vars.cc
  multi_option.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/common/user_registration.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc
  LINK_LIBRARIES mysqlclient ${EDITLINE_LIBRARY}
  )

MYSQL_ADD_EXECUTABLE(mysqltest
  mysqltest.cc
  mysqltest/error_names.cc
  mysqltest/expected_errors.cc
  mysqltest/expected_warnings.cc
  mysqltest/logfile.cc
  mysqltest/regular_expressions.cc
  mysqltest/secondary_engine.cc
  mysqltest/utils.cc
  client_query_attributes.cc

  COMPONENT Test
  DEPENDENCIES GenError GenClientError
  ENABLE_EXPORTS
  LINK_LIBRARIES mysqlclient
  )

IF(MY_COMPILER_IS_GNU AND (WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO))
  TARGET_LINK_OPTIONS(mysqltest PRIVATE -Wno-error=stringop-overflow)
ENDIF()

MYSQL_ADD_EXECUTABLE(mysqlcheck
  check/mysqlcheck.cc
  check/mysqlcheck_core.cc
  multi_factor_passwordopt-vars.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysqldump
  mysqldump.cc
  multi_factor_passwordopt-vars.cc
  multi_option.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysqlimport
  mysqlimport.cc
  multi_factor_passwordopt-vars.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysqlshow
  mysqlshow.cc
  multi_factor_passwordopt-vars.cc
  LINK_LIBRARIES mysqlclient
  )

OPTION(WITH_JSON_BINLOG_LIBRARY
  "Build and install standalone json_binlog library" OFF)

IF(WITH_JSON_BINLOG_LIBRARY)
  UNSET(INSTALL_JSON_BINLOG_LIBRARY)
  INSTALL(FILES
    ../sql-common/json_binary.h
    ../sql-common/json_error_handler.h
    DESTINATION ${INSTALL_INCLUDEDIR}/sql-common
    COMPONENT Development
    )
ELSE()
  SET(INSTALL_JSON_BINLOG_LIBRARY SKIP_INSTALL)
ENDIF()

# Set library version to match server version.
IF(WIN32)
  # Windows wants only major.minor, but our MINOR_VERSION is zero anyways:
  SET(json_binlog_VERSION "${MAJOR_VERSION}.${PATCH_VERSION}")
ELSE()
  SET(json_binlog_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
ENDIF()

# This is a standalone library to de-serialize json data.
# The public interface is in ../sql/json_binary.h which provides functions
# for parsing binary data, and converting it to Value objects.
# There are also utility functions to convert Value objects to JSON strings.
# We add enough source files to make it a complete standalone library,
# which can be tested by our ordinary mtr tests for the 'mysqlbinlog' client.
# Some detailed interface tests are found as unit tests.
#
# Always build it, but do not *use* it for mysqlbinlog unless
# WITH_JSON_BINLOG_LIBRARY=ON
ADD_SHARED_LIBRARY(json_binlog
  ${CMAKE_SOURCE_DIR}/sql-common/json_binary.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_dom.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_path.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_syntax_check.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc

  COMPILE_DEFINITIONS DISABLE_PSI_MUTEX EXPORT_JSON_FUNCTIONS
  DEPENDENCIES GenError
  ${INSTALL_JSON_BINLOG_LIBRARY}
  LINK_LIBRARIES mysys extra::rapidjson ext::zlib decimal
  LINUX_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/json_binlog.ver"
  VERSION "${json_binlog_VERSION}"
  NO_UNDEFINED
  )

ADD_CONVENIENCE_LIBRARY(json_client_library
  ${CMAKE_SOURCE_DIR}/sql-common/json_binary.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_dom.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_path.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_schema.cc
  ${CMAKE_SOURCE_DIR}/sql-common/json_syntax_check.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc

  LINK_LIBRARIES ext::zlib decimal
  COMPILE_DEFINITIONS
    DISABLE_PSI_MUTEX
    $<TARGET_PROPERTY:extra::rapidjson,INTERFACE_COMPILE_DEFINITIONS>
  SYSTEM_INCLUDE_DIRECTORIES
    $<TARGET_PROPERTY:extra::rapidjson,INTERFACE_INCLUDE_DIRECTORIES>
  DEPENDENCIES GenError
  )

ADD_CONVENIENCE_LIBRARY(decimal
  ${CMAKE_SOURCE_DIR}/sql-common/my_decimal.cc
  )

SET(JSON_BINLOG_LIBS_TO_MERGE
  json_client_library
  mysys
  mytime
  strings
  decimal
  )

MERGE_CONVENIENCE_LIBRARIES(json_binlog_static
  ${JSON_BINLOG_LIBS_TO_MERGE}
  COMPONENT Development
  ${INSTALL_JSON_BINLOG_LIBRARY}
  )

# This is a test executable which verifies that Json_wrapper::seek,
# Json_dom::seek and Json_dom::parse functions are visible and can be called.
MYSQL_ADD_EXECUTABLE(json_client_library_main
  json_client_library_main.cc
  INCLUDE_DIRECTORIES ../sql
  LINK_LIBRARIES json_client_library  strings mysys decimal
  SKIP_INSTALL
  )

# A test executable which does nothing, except verify that symbols are visible.
# To test standalone build:
#   cmake -DINSTALL_MYSQLTESTDIR=0 -DWITH_JSON_BINLOG_LIBRARY=1
#   make install DESTDIR=/tmp/foo
#   g++ -I/tmp/foo/usr/local/mysql/include json_binlog_main.cc
#       -L/tmp/foo/usr/local/mysql/lib -Wl,-R,/tmp/foo/usr/local/mysql/lib
#       -ljson_binlog
MYSQL_ADD_EXECUTABLE(json_binlog_main
  json_binlog_main.cc
  INCLUDE_DIRECTORIES ../sql ../sql-common
  LINK_LIBRARIES json_binlog
  SKIP_INSTALL
  )

# Same test executable, linked statically.
# To test standalone build:
#   g++ -I/tmp/foo/usr/local/mysql/include json_binlog_main.cc
#       -L/tmp/foo/usr/local/mysql/lib
#       -ljson_binlog_static -lpthread
MYSQL_ADD_EXECUTABLE(json_binlog_main_static
  json_binlog_main.cc
  INCLUDE_DIRECTORIES ../sql ../sql-common
  LINK_LIBRARIES json_binlog_static
  SKIP_INSTALL
  )

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  TARGET_COMPILE_OPTIONS(json_binlog_main PRIVATE "${HAS_WARN_FLAG}")
  TARGET_COMPILE_OPTIONS(json_binlog_main_static PRIVATE "${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wunused-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  TARGET_COMPILE_OPTIONS(json_binlog_main PRIVATE "${HAS_WARN_FLAG}")
  TARGET_COMPILE_OPTIONS(json_binlog_main_static PRIVATE "${HAS_WARN_FLAG}")
ENDIF()

SET(MYSQLBINLOG_SOURCES
  mysqlbinlog.cc
  ${CMAKE_SOURCE_DIR}/sql/log_event.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_utility.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_tsid_map.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_misc.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_set.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_specification.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_tblmap.cc
  ${CMAKE_SOURCE_DIR}/sql/basic_istream.cc
  ${CMAKE_SOURCE_DIR}/sql/binlog_istream.cc
  ${CMAKE_SOURCE_DIR}/sql/binlog_reader.cc
  ${CMAKE_SOURCE_DIR}/sql/stream_cipher.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_log_encryption.cc
  )

# The client version of log_event.cc has false positives.
# Downgrade from error to warning:
IF(MY_COMPILER_IS_GNU AND CMAKE_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO")
  ADD_COMPILE_FLAGS(${CMAKE_SOURCE_DIR}/sql/log_event.cc
    COMPILE_FLAGS
    " -Wno-error=uninitialized"
    " -Wno-error=maybe-uninitialized"
    )
ENDIF()

# duplicate explicit instantiation of 'net_field_length_checked'
# ignored as a Microsoft extension
IF(WIN32_CLANG)
  ADD_COMPILE_FLAGS(${CMAKE_SOURCE_DIR}/sql/log_event.cc
    COMPILE_FLAGS " -Wno-microsoft-template")
ENDIF()

SET(MYSQLBINLOG_LIBRARIES
  mysql_binlog_event
  mysqlclient
  )

IF(WITH_JSON_BINLOG_LIBRARY)
  LIST(APPEND MYSQLBINLOG_LIBRARIES json_binlog)
ELSE()
  LIST(APPEND MYSQLBINLOG_LIBRARIES json_binlog_static)
ENDIF()

MYSQL_ADD_EXECUTABLE(mysqlbinlog
  ${MYSQLBINLOG_SOURCES}
  DEPENDENCIES GenError
  LINK_LIBRARIES ${MYSQLBINLOG_LIBRARIES}
  )

# __builtin_strncpy specified bound depends on the length of the source argument
# mysqlbinlog.cc:2169:22: note: length computed here
IF((WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO) AND
    MY_COMPILER_IS_GNU AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11)
  TARGET_LINK_OPTIONS(mysqlbinlog PRIVATE -Wno-stringop-truncation)
ENDIF()

IF(WITH_JSON_BINLOG_LIBRARY)
  IF(APPLE)
    ADD_CUSTOM_COMMAND(TARGET mysqlbinlog POST_BUILD
      COMMAND install_name_tool -change
      "@rpath/$<TARGET_FILE_NAME:json_binlog>"
      "@loader_path/../lib/$<TARGET_FILE_NAME:json_binlog>"
      "$<TARGET_FILE:mysqlbinlog>"
      )
  ELSEIF(UNIX)
    ADD_INSTALL_RPATH(mysqlbinlog "\$ORIGIN/../${INSTALL_LIBDIR}")
  ENDIF()
ENDIF()

TARGET_COMPILE_DEFINITIONS(mysqlbinlog PRIVATE DISABLE_PSI_MUTEX)
TARGET_INCLUDE_DIRECTORIES(mysqlbinlog PRIVATE ${CMAKE_SOURCE_DIR}/sql)

MYSQL_ADD_EXECUTABLE(mysqladmin
  mysqladmin.cc
  multi_factor_passwordopt-vars.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysqlslap
  mysqlslap.cc
  multi_factor_passwordopt-vars.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysql_config_editor
  mysql_config_editor.cc
  LINK_LIBRARIES mysqlclient
  )
MYSQL_ADD_EXECUTABLE(mysql_secure_installation
  mysql_secure_installation.cc
  LINK_LIBRARIES mysqlclient
  )

# "WIN32" also covers 64 bit. "echo" is used in some files below "mysql-test/".
IF(WIN32)
  MYSQL_ADD_EXECUTABLE(echo echo.cc)
ENDIF(WIN32)
