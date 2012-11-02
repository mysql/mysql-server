# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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


# Any files in this directory are auxiliary files for Solaris "pkg" packages.
# They will be configured during "pkg" creation, not during (binary) build.

# Currently, this expands to "support-files/" in most layouts,
# but to "/usr/share/mysql/" in a RPM.
# It is important not to pollute "/usr/bin".
SET(inst_location ${INSTALL_SUPPORTFILESDIR})

FOREACH(script  postinstall-solaris)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${script}.sh 
                 ${CMAKE_CURRENT_BINARY_DIR}/${script} COPYONLY )

  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${script} 
     DESTINATION ${inst_location}/solaris COMPONENT Server_Scripts
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ENDFOREACH()
