# Copyright (c) 2017, MariaDB
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

FIND_PROGRAM(CHECKMODULE checkmodule)
FIND_PROGRAM(SEMODULE_PACKAGE semodule_package)
MARK_AS_ADVANCED(CHECKMODULE SEMODULE_PACKAGE)

SET(params DESTINATION ${INSTALL_SUPPORTFILESDIR}/SELinux COMPONENT SupportFiles)

IF(CHECKMODULE AND SEMODULE_PACKAGE)
  FOREACH(pol centos6-mariadb)
    SET(src ${CMAKE_CURRENT_SOURCE_DIR}/${pol}.te)
    SET(mod ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${pol}-pp.dir/${pol}.mod)
    SET(out ${CMAKE_CURRENT_BINARY_DIR}/${pol}.pp)
    ADD_CUSTOM_COMMAND(OUTPUT ${out}
      COMMAND ${CHECKMODULE} -M -m ${src} -o ${mod}
      COMMAND ${SEMODULE_PACKAGE} -m ${mod} -o ${out}
      DEPENDS ${src})
    ADD_CUSTOM_TARGET(${pol}-pp ALL DEPENDS ${out})
    INSTALL(FILES ${out} ${params})
  ENDFOREACH()
ENDIF()
INSTALL(FILES centos6-mariadb.te rhel4-mysql.fc rhel4-mysql.te ${params})
