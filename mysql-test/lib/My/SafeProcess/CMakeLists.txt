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

IF (INSTALL_MYSQLTESTDIR)
  SET(INSTALL_ARGS
    DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess"
    COMPONENT Test
    )
ENDIF()

IF (WIN32)
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process_win.cc ${INSTALL_ARGS}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  MYSQL_ADD_EXECUTABLE(my_safe_kill safe_kill_win.cc ${INSTALL_ARGS}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ELSE()
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process.cc ${INSTALL_ARGS}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDIF()

IF (NOT INSTALL_MYSQLTESTDIR)
  RETURN()
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
