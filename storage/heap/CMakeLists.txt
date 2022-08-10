# Copyright (c) 2006, 2022, Oracle and/or its affiliates.
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

SET(HEAP_SOURCES
  # _check.cc only for debug, see below.
  _rectest.cc
  hp_block.cc
  hp_clear.cc
  hp_close.cc
  hp_create.cc
  hp_delete.cc
  hp_extra.cc
  hp_hash.cc
  hp_info.cc
  hp_open.cc
  hp_panic.cc
  hp_rename.cc
  hp_rfirst.cc
  hp_rkey.cc
  hp_rlast.cc
  hp_rnext.cc
  hp_rprev.cc
  hp_rrnd.cc
  hp_scan.cc
  hp_static.cc
  hp_update.cc
  hp_write.cc
  )

IF(NOT BUILD_IS_SINGLE_CONFIG OR CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
  LIST(APPEND HEAP_SOURCES _check.cc)
ENDIF()

ADD_LIBRARY(heap_library STATIC ${HEAP_SOURCES})
ADD_DEPENDENCIES(heap_library GenError)
TARGET_LINK_LIBRARIES(heap_library mysys)

MYSQL_ADD_PLUGIN(heap ha_heap.cc
  STORAGE_ENGINE MANDATORY
  LINK_LIBRARIES heap_library
  )

IF(WITH_UNIT_TESTS)
  MYSQL_ADD_EXECUTABLE(hp_test1 hp_test1.cc _check.cc
    ADD_TEST hp_test1
    LINK_LIBRARIES heap_library
    )
  MYSQL_ADD_EXECUTABLE(hp_test2 hp_test2.cc _check.cc hp_rsame.cc
    ADD_TEST hp_test2
    LINK_LIBRARIES heap_library
    )
ENDIF()
