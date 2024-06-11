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

MYSQL_ADD_EXECUTABLE(mysql_client_test mysql_client_test.cc COMPONENT Test)
TARGET_LINK_LIBRARIES(mysql_client_test mysqlclient extra::rapidjson)

# We *do* calculate that there is space, see:
# printf("Warning: MYSQL_TMP_DIR is too long. Logging is disabled.\n");
IF(MY_COMPILER_IS_GNU AND CMAKE_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO")
  ADD_COMPILE_FLAGS(mysql_client_test.cc
    COMPILE_FLAGS "-Wno-error=format-truncation")
ENDIF()

IF(WITH_UNIT_TESTS)
  MYSQL_ADD_EXECUTABLE(bug25714 bug25714.cc SKIP_INSTALL EXCLUDE_FROM_PGO)
  TARGET_LINK_LIBRARIES(bug25714 mysqlclient)
ENDIF()
