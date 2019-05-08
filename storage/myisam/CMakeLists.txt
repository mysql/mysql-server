# Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.
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

ADD_CONVENIENCE_LIBRARY(myisam_sys
  my_lock.cc
  my_redel.cc
  queues.cc
)

SET(MYISAM_SOURCES  ft_boolean_search.cc ft_nlq_search.cc ft_parser.cc ft_static.cc
                                ha_myisam.cc
				ft_stopwords.cc ft_update.cc mi_cache.cc mi_changed.cc mi_check.cc
				mi_checksum.cc mi_close.cc mi_create.cc mi_dbug.cc mi_delete.cc
				mi_delete_all.cc mi_delete_table.cc mi_dynrec.cc mi_extra.cc mi_info.cc
				mi_key.cc mi_keycache.cc mi_locking.cc mi_log.cc mi_open.cc
				mi_packrec.cc mi_page.cc mi_panic.cc mi_preload.cc mi_range.cc mi_rename.cc
				mi_rfirst.cc mi_rlast.cc mi_rnext.cc mi_rnext_same.cc mi_rprev.cc mi_rrnd.cc
				mi_rsame.cc mi_rsamepos.cc mi_scan.cc mi_search.cc mi_static.cc mi_statrec.cc
				mi_unique.cc mi_update.cc mi_write.cc rt_index.cc rt_key.cc rt_mbr.cc
				rt_split.cc sort.cc sp_key.cc mi_extrafunc.h myisamdef.h
				rt_index.h mi_rkey.cc)

# MyISAM relies heavily on memset/memcpy of structs, including non-POD ones.
# As MyISAM is not maintained anymore, it is not worth fixing these,
# so we turn off the warning instead.
IF(CMAKE_COMPILER_IS_GNUCXX)
  ADD_COMPILE_FLAGS(
    ${MYISAM_SOURCES}
    myisam_ftdump.cc
    myisamchk.cc
    myisamlog.cc
    myisampack.cc
    COMPILE_FLAGS "-Wno-class-memaccess")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wcast-function-type" HAVE_CAST_FUNCTION_TYPE)
IF(HAVE_CAST_FUNCTION_TYPE)
  ADD_COMPILE_FLAGS(
    mi_write.cc
    myisamlog.cc
    COMPILE_FLAGS "-Wno-cast-function-type")
ENDIF()

MYSQL_ADD_PLUGIN(myisam ${MYISAM_SOURCES} 
  STORAGE_ENGINE 
  MANDATORY
  LINK_LIBRARIES mysys myisam_sys
)

ADD_CONVENIENCE_LIBRARY(myisam_nopfs ${MYISAM_SOURCES})
ADD_DEPENDENCIES(myisam_nopfs GenError)
TARGET_LINK_LIBRARIES(myisam_nopfs mysys myisam_sys)
SET(MYISAM_NOPFS myisam_nopfs)

MYSQL_ADD_EXECUTABLE(myisam_ftdump myisam_ftdump.cc)
TARGET_LINK_LIBRARIES(myisam_ftdump ${MYISAM_NOPFS} mysys_ssl)

MYSQL_ADD_EXECUTABLE(myisamchk myisamchk.cc)
TARGET_LINK_LIBRARIES(myisamchk ${MYISAM_NOPFS} mysys_ssl)

MYSQL_ADD_EXECUTABLE(myisamlog myisamlog.cc)
TARGET_LINK_LIBRARIES(myisamlog ${MYISAM_NOPFS} ${LIBSOCKET})

MYSQL_ADD_EXECUTABLE(myisampack myisampack.cc)
TARGET_LINK_LIBRARIES(myisampack ${MYISAM_NOPFS} mysys_ssl)

IF (MSVC)
  SET_TARGET_PROPERTIES(myisamchk myisampack PROPERTIES LINK_FLAGS "setargv.obj")
ENDIF()
