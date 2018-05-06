# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

SET (dirs 
danish
german 
slovak
dutch
greek
norwegian
spanish
english
hungarian
norwegian-ny
swedish
italian
polish
ukrainian
japanese
portuguese
romanian
estonian 
korean
russian
czech
french
serbian
bulgarian
)

SET(files 
 errmsg-utf8.txt
 dictionary.txt
)

FOREACH (dir ${dirs})
  INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/${dir}
          DESTINATION ${INSTALL_MYSQLSHAREDIR}
          COMPONENT Server
  )
ENDFOREACH()

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/charsets
        DESTINATION ${INSTALL_MYSQLSHAREDIR}
        COMPONENT Server
        PATTERN "languages.html" EXCLUDE
)

# Don't copy if we're building in-source.
IF (NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  CONFIGURE_FILE(dictionary.txt ${CMAKE_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/dictionary.txt)
ENDIF()

INSTALL(FILES ${files}
        DESTINATION ${INSTALL_MYSQLSHAREDIR}
        COMPONENT Server
)
