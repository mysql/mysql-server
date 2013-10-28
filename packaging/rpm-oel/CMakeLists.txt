# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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
  IF("${VERSION}" MATCHES "-ndb-")
    STRING(REGEX REPLACE "^.*-ndb-" "" NDBVERSION "${VERSION}")
    SET(SPECFILENAME "mysql-cluster-${NDBVERSION}.spec")
  ENDIF()

  # Left in current directory, to be taken during build
  CONFIGURE_FILE(mysql.spec.in ${CMAKE_CURRENT_BINARY_DIR}/${SPECFILENAME} @ONLY)

  FOREACH(fedfile my.cnf my_config.h mysql_config.sh
          mysqld.service mysql-systemd-start mysql.conf
          mysql-5.6.14-mysql-install.patch
          filter-requires.sh filter-provides.sh
          mysql-embedded-check.c mysql.init)
    CONFIGURE_FILE(${fedfile} ${CMAKE_CURRENT_BINARY_DIR}/${fedfile} COPYONLY)
  ENDFOREACH()
ENDIF()

