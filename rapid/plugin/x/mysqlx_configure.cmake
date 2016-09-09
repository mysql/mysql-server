# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

FUNCTION(GENERATE_XERRORS FILE_NAME VARIABLE_NAME)
  FILE(READ ${FILE_NAME} CONTENT)
  STRING(REGEX MATCHALL "#define ER_X_[A-Z_]+[ ]+[0-9]+\n" TMP ${CONTENT})
  STRING(REGEX REPLACE ";" "" RESULT ${TMP})
  SET(${VARIABLE_NAME} ${RESULT} PARENT_SCOPE)
ENDFUNCTION()

GENERATE_XERRORS(${MYSQLX_PROJECT_DIR}/ngs/include/ngs/ngs_error.h NGS_ERROR)
GENERATE_XERRORS(${MYSQLX_PROJECT_DIR}/src/xpl_error.h XPL_ERROR)

CONFIGURE_FILE(${MYSQLX_PROJECT_DIR}/src/mysqlx_error.h.in 
               ${CMAKE_CURRENT_BINARY_DIR}/generated/mysqlx_error.h)

CONFIGURE_FILE(${MYSQLX_PROJECT_DIR}/src/mysqlx_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/mysqlx_version.h )

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/mysqlx_error.h
        DESTINATION ${INSTALL_INCLUDEDIR}
        COMPONENT Developement)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/mysqlx_version.h
        DESTINATION ${INSTALL_INCLUDEDIR}
        COMPONENT Developement)
