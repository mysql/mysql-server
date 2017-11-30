# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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


IF(UNIX)
  SET(prefix ${CMAKE_INSTALL_PREFIX})

  SET(SPECFILENAME "mysql.spec")

  IF ("${PIDFILE_RPM}" STREQUAL "")
    SET(PIDFILE_RPM  "/var/run/mysqld/mysqld.pid")
  ENDIF()

  IF ("${LOGFILE_RPM}" STREQUAL "")
    SET(LOGFILE_RPM "/var/log/mysqld.log")
  ENDIF()

  # Left in current directory, to be taken during build
  CONFIGURE_FILE(mysql.spec.in ${CMAKE_CURRENT_BINARY_DIR}/${SPECFILENAME} @ONLY)

  FOREACH(fedfile my.cnf)
    CONFIGURE_FILE(${fedfile}.in ${CMAKE_CURRENT_BINARY_DIR}/${fedfile} @ONLY)
  ENDFOREACH()
ENDIF()

