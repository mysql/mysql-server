# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET(REWRITER_SOURCES
  rewriter_plugin.cc
  rewriter_udf.cc
  rule.cc
  rewriter.cc
  services.cc)

MYSQL_ADD_PLUGIN(rewriter ${REWRITER_SOURCES}
                 MODULE_ONLY MODULE_OUTPUT_NAME "rewriter")

ADD_DEFINITIONS(-DMYSQL_SERVER)
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="Rewriter")

IF(WIN32)
  SET(DYNLIB_EXTENSION "dll")
  IF(MSVC)
   SET_TARGET_PROPERTIES(rewriter
      PROPERTIES LINK_FLAGS
      "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/rewriter_udf.def")
  ENDIF()
ELSE()
  SET(DYNLIB_EXTENSION "so")
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
