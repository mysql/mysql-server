# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

MYSQL_ADD_PLUGIN(rewriter
  rewriter.cc
  rewriter_plugin.cc
  rewriter_udf.cc
  rule.cc
  services.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "rewriter"
  WIN_DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rewriter_udf.def"
  )

# MYSQL_ADD_PLUGIN may have decided not to build it.
IF(NOT TARGET rewriter)
  RETURN()
ENDIF()

DOWNGRADE_STRINGOP_WARNINGS(rewriter)

ADD_DEFINITIONS(-DMYSQL_SERVER)
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="Rewriter")

# We should use CMAKE_SHARED_LIBRARY_SUFFIX, but we do not use .dylib on Mac.
IF(WIN32)
  SET(DYNLIB_EXTENSION "dll")
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
