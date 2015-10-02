# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

SET(REWRITER_SOURCES
  rewriter_plugin.cc
  rewriter_udf.cc
  rule.cc
  rewriter.cc
  services.cc)

IF (DISABLE_SHARED)
  RETURN()
ENDIF()

MYSQL_ADD_PLUGIN(rewriter ${REWRITER_SOURCES}
                 MODULE_ONLY MODULE_OUTPUT_NAME "rewriter")

IF(WIN32 OR HAVE_DLOPEN)
  IF(WIN32)
    SET(DYNLIB_EXTENSION "dll")
    IF(MSVC)
      SET_TARGET_PROPERTIES(rewriter
        PROPERTIES LINK_FLAGS
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/rewriter_udf.def")
    ENDIF()
  ELSE()
    SET(DYNLIB_EXTENSION "so")
    SET_TARGET_PROPERTIES(rewriter
      PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
  ENDIF()
ENDIF()



#
# This installs all the scripts.
#
FOREACH(file
    install_rewriter
    uninstall_rewriter)

  CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/${file}.sql.in
    ${CMAKE_CURRENT_BINARY_DIR}/${file}.sql
    )

  INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${file}.sql
    DESTINATION ${INSTALL_MYSQLSHAREDIR}
    COMPONENT SupportFiles
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

ENDFOREACH()
