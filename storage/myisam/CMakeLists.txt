# Copyright (c) 2006, 2023, Oracle and/or its affiliates.
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

SET(MYISAM_SOURCES
  # Originally part of mysys.
  my_lock.cc
  my_redel.cc
  queues.cc
  # Implementation of myisam storage engine.
  ft_boolean_search.cc
  ft_nlq_search.cc
  ft_parser.cc
  ft_static.cc
  ft_stopwords.cc
  ft_update.cc
  mi_cache.cc
  mi_changed.cc
  mi_check.cc
  mi_checksum.cc
  mi_close.cc
  mi_create.cc
  mi_dbug.cc
  mi_delete.cc
  mi_delete_all.cc
  mi_delete_table.cc
  mi_dynrec.cc
  mi_extra.cc
  mi_info.cc
  mi_key.cc
  mi_keycache.cc
  mi_locking.cc
  mi_log.cc
  mi_open.cc
  mi_packrec.cc
  mi_page.cc
  mi_panic.cc
  mi_preload.cc
  mi_range.cc
  mi_rename.cc
  mi_rfirst.cc
  mi_rkey.cc
  mi_rlast.cc
  mi_rnext.cc
  mi_rnext_same.cc
  mi_rprev.cc
  mi_rrnd.cc
  mi_rsame.cc
  mi_rsamepos.cc
  mi_scan.cc
  mi_search.cc
  mi_static.cc
  mi_statrec.cc
  mi_unique.cc
  mi_update.cc
  mi_write.cc
  rt_index.cc
  rt_key.cc
  rt_mbr.cc
  rt_split.cc
  sort.cc
  sp_key.cc
  )

# MyISAM relies heavily on memset/memcpy of structs, including non-POD ones.
# As MyISAM is not maintained anymore, it is not worth fixing these,
# so we turn off the warning instead.
IF(MY_COMPILER_IS_GNU)
  ADD_COMPILE_FLAGS(
    ${MYISAM_SOURCES}
    myisam_ftdump.cc
    myisamchk.cc
    myisamlog.cc
    myisampack.cc
    COMPILE_FLAGS "-Wno-class-memaccess")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wcast-function-type" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(
    mi_write.cc
    myisamlog.cc
    COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

ADD_LIBRARY(myisam_library STATIC ${MYISAM_SOURCES})
ADD_DEPENDENCIES(myisam_library GenError)
TARGET_LINK_LIBRARIES(myisam_library mysys extra::rapidjson)

MYSQL_ADD_PLUGIN(myisam ha_myisam.cc
  STORAGE_ENGINE
  MANDATORY
  LINK_LIBRARIES myisam_library
)

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_EXECUTABLE(myisam_ftdump myisam_ftdump.cc
  LINK_LIBRARIES myisam_library)

MYSQL_ADD_EXECUTABLE(myisamchk myisamchk.cc
  LINK_LIBRARIES myisam_library)

MYSQL_ADD_EXECUTABLE(myisamlog myisamlog.cc
  LINK_LIBRARIES myisam_library ${LIBSOCKET})

MYSQL_ADD_EXECUTABLE(myisampack myisampack.cc
  LINK_LIBRARIES myisam_library)

IF (WIN32)
  MY_TARGET_LINK_OPTIONS(myisamchk  "setargv.obj")
  MY_TARGET_LINK_OPTIONS(myisampack "setargv.obj")
ENDIF()
