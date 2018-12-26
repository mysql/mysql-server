# Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

SET(HEAP_PLUGIN_STATIC  "heap")
SET(HEAP_PLUGIN_MANDATORY  TRUE)

SET(HEAP_SOURCES  _check.c _rectest.c hp_block.c hp_clear.c hp_close.c hp_create.c
				ha_heap.cc
				hp_delete.c hp_extra.c hp_hash.c hp_info.c hp_open.c hp_panic.c
				hp_rename.c hp_rfirst.c hp_rkey.c hp_rlast.c hp_rnext.c hp_rprev.c
				hp_rrnd.c hp_rsame.c hp_scan.c hp_static.c hp_update.c hp_write.c)

MYSQL_ADD_PLUGIN(heap ${HEAP_SOURCES} STORAGE_ENGINE MANDATORY RECOMPILE_FOR_EMBEDDED)

IF(WITH_UNIT_TESTS AND WITH_EMBEDDED_SERVER)
  ADD_EXECUTABLE(hp_test1 hp_test1.c)
  TARGET_LINK_LIBRARIES(hp_test1 heap_embedded mysys)
  ADD_TEST(hp_test1 hp_test1)

  ADD_EXECUTABLE(hp_test2 hp_test2.c)
  TARGET_LINK_LIBRARIES(hp_test2 heap_embedded mysys)
  ADD_TEST(hp_test2 hp_test2)
ENDIF()
