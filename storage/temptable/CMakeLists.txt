# Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

MSVC_CPPCHECK_DISABLE()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

SET(TEMPTABLE_SOURCES
  src/allocator.cc
  src/block.cc
  src/column.cc
  src/handler.cc
  src/index.cc
  src/indexed_cells.cc
  src/memutils.cc
  src/plugin.cc
  src/result.cc
  src/row.cc
  src/table.cc
  )

IF(NOT LINUX)
  LIST(REMOVE_ITEM TEMPTABLE_SOURCES src/memutils.cc)
ENDIF()

MYSQL_ADD_PLUGIN(temptable
  ${TEMPTABLE_SOURCES}
  STORAGE_ENGINE
  MANDATORY
  LINK_LIBRARIES extra::rapidjson
)

# Only used for debugging.
MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(src/result.cc COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()
