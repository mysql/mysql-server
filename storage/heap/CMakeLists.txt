# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

SET(HEAP_PLUGIN_STATIC  "heap")
SET(HEAP_PLUGIN_MANDATORY  TRUE)

SET(HEAP_SOURCES  _check.cc _rectest.cc hp_block.cc hp_clear.cc hp_close.cc hp_create.cc
				ha_heap.cc
				hp_delete.cc hp_extra.cc hp_hash.cc hp_info.cc hp_open.cc hp_panic.cc
				hp_rename.cc hp_rfirst.cc hp_rkey.cc hp_rlast.cc hp_rnext.cc hp_rprev.cc
				hp_rrnd.cc hp_rsame.cc hp_scan.cc hp_static.cc hp_update.cc hp_write.cc)

MYSQL_ADD_PLUGIN(heap ${HEAP_SOURCES} STORAGE_ENGINE MANDATORY)

IF(WITH_UNIT_TESTS)
  ADD_CONVENIENCE_LIBRARY(heap_nopfs ${HEAP_SOURCES})
  ADD_DEPENDENCIES(heap_nopfs GenError)

  MYSQL_ADD_EXECUTABLE(hp_test1 hp_test1.cc ADD_TEST hp_test1)
  TARGET_LINK_LIBRARIES(hp_test1 heap_nopfs mysys)

  MYSQL_ADD_EXECUTABLE(hp_test2 hp_test2.cc ADD_TEST hp_test2)
  TARGET_LINK_LIBRARIES(hp_test2 heap_nopfs mysys)
ENDIF()
