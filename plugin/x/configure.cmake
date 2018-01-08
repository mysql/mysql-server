# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

FUNCTION(GENERATE_XERRORS FILE_NAME VARIABLE_NAME_DEFINES VARIABLE_NAME_ENTRIES)
  FILE(READ ${FILE_NAME} CONTENT)
  STRING(REGEX MATCHALL "#define ER_X_[A-Z_]+[ ]+[0-9]+\n" TMP ${CONTENT})
  STRING(REGEX REPLACE ";" "" RESULT_DEFINES ${TMP})
  STRING(REGEX REPLACE "#define (ER_X_[A-Z_]+)[ ]+[0-9]+\n" "  {\"\\1\", \\1, \"\", NULL, NULL, 0 },\n" RESULT_ENTRIES ${TMP})

  SET(${VARIABLE_NAME_DEFINES} ${RESULT_DEFINES} PARENT_SCOPE)
  SET(${VARIABLE_NAME_ENTRIES} ${RESULT_ENTRIES} PARENT_SCOPE)
ENDFUNCTION()

SET(MYSQLX_GENERATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

GENERATE_XERRORS(${MYSQLX_PROJECT_DIR}/ngs/include/ngs/ngs_error.h NGS_ERROR NGS_ERROR_NAMES)
GENERATE_XERRORS(${MYSQLX_PROJECT_DIR}/src/xpl_error.h XPL_ERROR XPL_ERROR_NAMES)

CONFIGURE_FILE(${MYSQLX_PROJECT_DIR}/src/mysqlx_error.h.in
               ${MYSQLX_GENERATE_DIR}/mysqlx_error.h)

CONFIGURE_FILE(${MYSQLX_PROJECT_DIR}/src/mysqlx_ername.h.in
               ${MYSQLX_GENERATE_DIR}/mysqlx_ername.h)

CONFIGURE_FILE(${MYSQLX_PROJECT_DIR}/src/mysqlx_version.h.in
               ${MYSQLX_GENERATE_DIR}/mysqlx_version.h )

INSTALL(FILES ${MYSQLX_GENERATE_DIR}/mysqlx_error.h
        DESTINATION ${INSTALL_INCLUDEDIR}
        COMPONENT Development)

INSTALL(FILES ${MYSQLX_GENERATE_DIR}/mysqlx_ername.h
        DESTINATION ${INSTALL_INCLUDEDIR}
        COMPONENT Development)

INSTALL(FILES ${MYSQLX_GENERATE_DIR}/mysqlx_version.h
        DESTINATION ${INSTALL_INCLUDEDIR}
        COMPONENT Development)
