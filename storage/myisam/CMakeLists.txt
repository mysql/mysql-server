# Copyright (C) 2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
INCLUDE("${PROJECT_SOURCE_DIR}/win/mysql_manifest.cmake")


SET(MYISAM_SOURCES  ft_boolean_search.c ft_nlq_search.c ft_parser.c ft_static.c ft_stem.c
                                ha_myisam.cc
				ft_stopwords.c ft_update.c mi_cache.c mi_changed.c mi_check.c
				mi_checksum.c mi_close.c mi_create.c mi_dbug.c mi_delete.c 
				mi_delete_all.c mi_delete_table.c mi_dynrec.c mi_extra.c mi_info.c
				mi_key.c mi_keycache.c mi_locking.c mi_log.c mi_open.c 
				mi_packrec.c mi_page.c mi_panic.c mi_preload.c mi_range.c mi_rename.c
				mi_rfirst.c mi_rlast.c mi_rnext.c mi_rnext_same.c mi_rprev.c mi_rrnd.c
				mi_rsame.c mi_rsamepos.c mi_scan.c mi_search.c mi_static.c mi_statrec.c
				mi_unique.c mi_update.c mi_write.c rt_index.c rt_key.c rt_mbr.c
				rt_split.c sort.c sp_key.c ft_eval.h myisamdef.h rt_index.h mi_rkey.c)

MYSQL_STORAGE_ENGINE(MYISAM)

IF(NOT SOURCE_SUBLIBS)
  ADD_EXECUTABLE(myisam_ftdump myisam_ftdump.c)
  TARGET_LINK_LIBRARIES(myisam_ftdump myisam mysys dbug strings zlib)

  ADD_EXECUTABLE(myisamchk myisamchk.c)
  TARGET_LINK_LIBRARIES(myisamchk myisam mysys dbug strings zlib)

  ADD_EXECUTABLE(myisamlog myisamlog.c)
  TARGET_LINK_LIBRARIES(myisamlog myisam mysys dbug strings zlib)

  ADD_EXECUTABLE(myisampack myisampack.c)
  TARGET_LINK_LIBRARIES(myisampack myisam mysys dbug strings zlib)

  SET_TARGET_PROPERTIES(myisamchk myisampack PROPERTIES LINK_FLAGS "setargv.obj")

  IF(EMBED_MANIFESTS)
    MYSQL_EMBED_MANIFEST("myisam_ftdump" "asInvoker")
    MYSQL_EMBED_MANIFEST("myisamchk" "asInvoker")
    MYSQL_EMBED_MANIFEST("myisamlog" "asInvoker")
    MYSQL_EMBED_MANIFEST("myisampack" "asInvoker")
  ENDIF(EMBED_MANIFESTS)

ENDIF(NOT SOURCE_SUBLIBS)
