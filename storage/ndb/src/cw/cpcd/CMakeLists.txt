# Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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
FOREACH(warning
    "strict-aliasing"
    "unused-parameter"
    "maybe-uninitialized"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
  )

ADD_EXECUTABLE(ndb_cpcd
  APIService.cpp
  CPCD.cpp
  Monitor.cpp
  Process.cpp
  common.cpp
  main.cpp
  )
SET_TARGET_PROPERTIES(ndb_cpcd PROPERTIES ENABLE_EXPORTS TRUE)

TARGET_LINK_LIBRARIES(ndb_cpcd ndbclient_static)

INSTALL(TARGETS ndb_cpcd DESTINATION bin)
