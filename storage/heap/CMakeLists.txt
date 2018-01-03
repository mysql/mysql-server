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

SET(HEAP_SOURCES  _check.c _rectest.c hp_block.c hp_clear.c hp_close.c hp_create.c
				ha_heap.cc
				hp_delete.c hp_extra.c hp_hash.c hp_info.c hp_open.c hp_panic.c
				hp_rename.c hp_rfirst.c hp_rkey.c hp_rlast.c hp_rnext.c hp_rprev.c
				hp_rrnd.c hp_rsame.c hp_scan.c hp_static.c hp_update.c hp_write.c)

MYSQL_ADD_PLUGIN(heap ${HEAP_SOURCES} STORAGE_ENGINE MANDATORY)

IF(WITH_UNIT_TESTS)
  ADD_CONVENIENCE_LIBRARY(heap_nopfs ${HEAP_SOURCES})
  ADD_DEPENDENCIES(heap_nopfs GenError)

  MYSQL_ADD_EXECUTABLE(hp_test1 hp_test1.c ADD_TEST hp_test1)
  TARGET_LINK_LIBRARIES(hp_test1 heap_nopfs mysys)

  MYSQL_ADD_EXECUTABLE(hp_test2 hp_test2.c ADD_TEST hp_test2)
  TARGET_LINK_LIBRARIES(hp_test2 heap_nopfs mysys)
ENDIF()
