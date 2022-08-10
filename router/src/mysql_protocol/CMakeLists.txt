# Copyright (c) 2016, 2022, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(GenerateExportHeader)

SET(include_dirs
  include
  )

ADD_LIBRARY(mysql_protocol INTERFACE)

TARGET_SOURCES(mysql_protocol
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/mysqlrouter/classic_protocol.h
  )

TARGET_INCLUDE_DIRECTORIES(mysql_protocol INTERFACE ${include_dirs})
# GENERATE_EXPORT_HEADER(mysql_protocol
#   EXPORT_FILE_NAME
#   ${CMAKE_CURRENT_BINARY_DIR}/include/mysqlrouter/mysql_protocol_export.h
#   )

IF(WITH_UNIT_TESTS)
  ADD_SUBDIRECTORY(tests/)
ENDIF()
