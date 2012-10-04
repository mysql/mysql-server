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


IF(UNIX)
  SET(prefix ${CMAKE_INSTALL_PREFIX})

  SET(SPECFILENAME "mysql.${VERSION}.spec")
  IF("${VERSION}" MATCHES "-ndb-")
    STRING(REGEX REPLACE "^.*-ndb-" "" NDBVERSION "${VERSION}")
    SET(SPECFILENAME "mysql-cluster-${NDBVERSION}.spec")
  ENDIF()

  # Left in current directory, to be taken during build
  CONFIGURE_FILE(mysql.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/${SPECFILENAME} @ONLY)

  FOREACH(ulnfile filter-requires-mysql.sh generate-tarball.sh my.cnf my_config.h
          mysql-5.5-errno.patch mysql-5.5-fix-tests.patch mysql-5.5-libdir.patch
          mysql-5.5-mtr1.patch mysql-5.5-stack-guard.patch mysql-5.5-testing.patch
          mysql-chain-certs.patch mysql-embedded-check.c mysql-expired-certs.patch
          mysql.init mysql-install-test.patch mysql-strmov.patch scriptstub.c
          README.mysql-docs)
    CONFIGURE_FILE(${ulnfile} ${CMAKE_CURRENT_BINARY_DIR}/${ulnfile} COPYONLY)
  ENDFOREACH()
ENDIF()

