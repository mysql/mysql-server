# Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
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
  INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${dir} 
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
  CONFIGURE_FILE(dictionary.txt ${CMAKE_BINARY_DIR}/sql/share/dictionary.txt)
ENDIF()

INSTALL(FILES ${files}
        DESTINATION ${INSTALL_MYSQLSHAREDIR}
        COMPONENT Server
)
