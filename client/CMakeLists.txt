# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

## Subdirectory with common client code.
ADD_SUBDIRECTORY(base)
## Subdirectory for mysqlpump code.
ADD_SUBDIRECTORY(dump)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

MYSQL_ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc ../sql-common/sql_string.cc pattern_matcher.cc)
TARGET_LINK_LIBRARIES(mysql mysqlclient)
IF(UNIX)
  TARGET_LINK_LIBRARIES(mysql ${EDITLINE_LIBRARY})
ENDIF(UNIX)

IF(NOT WITHOUT_SERVER)
  MYSQL_ADD_EXECUTABLE(mysql_upgrade upgrade/program.cc)
  TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient client_base mysqlcheck_core)
  ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs GenSysSchema GenBootstrapPriv)
ENDIF()

SET(MYSQLTEST_SRC
  mysqltest/error_names.cc
  mysqltest/expected_errors.cc
  mysqltest/secondary_engine.cc
)

MYSQL_ADD_EXECUTABLE(mysqltest mysqltest.cc ${MYSQLTEST_SRC} COMPONENT Test)
TARGET_LINK_LIBRARIES(mysqltest mysqlclient regex)
ADD_DEPENDENCIES(mysqltest GenError GenClientError)

ADD_CONVENIENCE_LIBRARY(mysqlcheck_core check/mysqlcheck_core.cc)
TARGET_LINK_LIBRARIES(mysqlcheck_core mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlcheck check/mysqlcheck.cc)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlcheck_core)

MYSQL_ADD_EXECUTABLE(mysqldump mysqldump.cc)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlimport mysqlimport.cc)
SET_SOURCE_FILES_PROPERTIES(mysqlimport.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlshow mysqlshow.cc)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc)
SET(MYSQLBINLOG_LIB_SOURCES
  ${CMAKE_SOURCE_DIR}/strings/decimal.cc
  ${CMAKE_SOURCE_DIR}/sql/json_binary.cc
  ${CMAKE_SOURCE_DIR}/sql/json_dom.cc
  ${CMAKE_SOURCE_DIR}/sql-common/sql_string.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_time.cc
  ${CMAKE_SOURCE_DIR}/sql/log_event.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_utility.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_sid_map.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_misc.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_set.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_gtid_specification.cc
  ${CMAKE_SOURCE_DIR}/sql/rpl_tblmap.cc
  ${CMAKE_SOURCE_DIR}/sql/basic_istream.cc
  ${CMAKE_SOURCE_DIR}/sql/binlog_istream.cc
  ${CMAKE_SOURCE_DIR}/sql/binlog_reader.cc
)
ADD_LIBRARY(mysqlbinlog_lib STATIC ${MYSQLBINLOG_LIB_SOURCES})
ADD_DEPENDENCIES(mysqlbinlog_lib GenError)
ADD_COMPILE_FLAGS(
  mysqlbinlog.cc
  ${MYSQLBINLOG_LIB_SOURCES}
  COMPILE_FLAGS
  "-I${CMAKE_SOURCE_DIR}/sql" "-DDISABLE_PSI_MUTEX"
)

TARGET_LINK_LIBRARIES(mysqlbinlog mysqlbinlog_lib)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient binlogevents_static)

MYSQL_ADD_EXECUTABLE(mysqladmin mysqladmin.cc)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlslap mysqlslap.cc)
SET_SOURCE_FILES_PROPERTIES(mysqlslap.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlslap mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_config_editor mysql_config_editor.cc)
TARGET_LINK_LIBRARIES(mysql_config_editor mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_secure_installation mysql_secure_installation.cc)
TARGET_LINK_LIBRARIES(mysql_secure_installation mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_ssl_rsa_setup mysql_ssl_rsa_setup.cc path.cc logger.cc)
TARGET_LINK_LIBRARIES(mysql_ssl_rsa_setup mysys mysys_ssl)
# Bug in /usr/lib/gcc-snapshot/lib/libstdc++.so ??
IF(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "9.0.0")
  TARGET_LINK_LIBRARIES(mysql_ssl_rsa_setup -static-libstdc++)
ENDIF()

# "WIN32" also covers 64 bit. "echo" is used in some files below "mysql-test/".
IF(WIN32)
  MYSQL_ADD_EXECUTABLE(echo echo.cc)
ENDIF(WIN32)
