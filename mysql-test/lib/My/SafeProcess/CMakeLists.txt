# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

IF (NOT INSTALL_MYSQLTESTDIR)
  RETURN()
ENDIF()

SET(INSTALL_ARGS 
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" 
  COMPONENT Test
)

IF (WIN32)
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process_win.cc ${INSTALL_ARGS})
  MYSQL_ADD_EXECUTABLE(my_safe_kill safe_kill_win.cc ${INSTALL_ARGS})
ELSE()
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process.cc ${INSTALL_ARGS})
ENDIF()

INSTALL(TARGETS my_safe_process
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
)

IF(WIN32)
  INSTALL(TARGETS my_safe_kill
    DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
  )
ENDIF()

INSTALL(FILES Base.pm
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
)
