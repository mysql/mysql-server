# Copyright (c) 2006 MySQL AB, 2009, 2010 Sun Microsystems, Inc.
# Use is subject to license terms.
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
)

SET(files 
 errmsg-utf8.txt
)

FOREACH (dir ${dirs})
 INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${dir} 
  DESTINATION  ${INSTALL_MYSQLSHAREDIR} COMPONENT Server)
ENDFOREACH()
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/charsets DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Server
  PATTERN "languages.html" EXCLUDE
)

INSTALL(FILES ${files} DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Server)
