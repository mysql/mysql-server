# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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

SET(MYISAM_SOURCES  ft_boolean_search.c ft_nlq_search.c ft_parser.c ft_static.c
                                ha_myisam.cc
				ft_stopwords.c ft_update.c mi_cache.c mi_changed.c mi_check.c
				mi_checksum.c mi_close.c mi_create.c mi_dbug.c mi_delete.c 
				mi_delete_all.c mi_delete_table.c mi_dynrec.c mi_extra.c mi_info.c
				mi_key.c mi_keycache.c mi_locking.c mi_log.c mi_open.c 
				mi_packrec.c mi_page.c mi_panic.c mi_preload.c mi_range.c mi_rename.c
				mi_rfirst.c mi_rlast.c mi_rnext.c mi_rnext_same.c mi_rprev.c mi_rrnd.c
				mi_rsame.c mi_rsamepos.c mi_scan.c mi_search.c mi_static.c mi_statrec.c
				mi_unique.c mi_update.c mi_write.c rt_index.c rt_key.c rt_mbr.c
				rt_split.c sort.c sp_key.c mi_extrafunc.h myisamdef.h
				rt_index.h mi_rkey.c)

MYSQL_ADD_PLUGIN(myisam ${MYISAM_SOURCES} 
  STORAGE_ENGINE 
  MANDATORY 
  RECOMPILE_FOR_EMBEDDED)

TARGET_LINK_LIBRARIES(myisam mysys)

MYSQL_ADD_EXECUTABLE(myisam_ftdump myisam_ftdump.c)
TARGET_LINK_LIBRARIES(myisam_ftdump myisam)

MYSQL_ADD_EXECUTABLE(myisamchk myisamchk.c)
TARGET_LINK_LIBRARIES(myisamchk myisam)

MYSQL_ADD_EXECUTABLE(myisamlog myisamlog.c)
TARGET_LINK_LIBRARIES(myisamlog myisam)

MYSQL_ADD_EXECUTABLE(myisampack myisampack.c)
TARGET_LINK_LIBRARIES(myisampack myisam)

IF(WITH_UNIT_TESTS AND FALSE)
  ADD_EXECUTABLE(mi_test1 mi_test1.c)
  TARGET_LINK_LIBRARIES(mi_test1 myisam)

  ADD_EXECUTABLE(mi_test2 mi_test2.c)
  TARGET_LINK_LIBRARIES(mi_test2 myisam)

  ADD_EXECUTABLE(mi_test3 mi_test3.c)
  TARGET_LINK_LIBRARIES(mi_test3  myisam)

  ADD_EXECUTABLE(sp_test sp_test.c)
  TARGET_LINK_LIBRARIES(sp_test myisam)

  ADD_EXECUTABLE(rt_test rt_test.c)
  TARGET_LINK_LIBRARIES(rt_test myisam)
ENDIF()
  
IF (MSVC)
  SET_TARGET_PROPERTIES(myisamchk myisampack PROPERTIES LINK_FLAGS "setargv.obj")
ENDIF()
