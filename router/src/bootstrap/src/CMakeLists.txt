# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

MYSQL_ADD_EXECUTABLE(mysqlrouter_bootstrap
  bootstrap_configurator.cc
  bootstrap_mysql_account.cc
  main.cc
  COMPONENT Router
  LINK_LIBRARIES
  harness-library
  router_lib
  router_frontend_lib
  mysql_rest_service_if
  mysql_rest_service_db
  ${CMAKE_DL_LIBS}
  )
TARGET_INCLUDE_DIRECTORIES(mysqlrouter_bootstrap
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../router/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../http/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../metadata_cache/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../router/src
  )

ADD_INSTALL_RPATH(mysqlrouter_bootstrap "${ROUTER_INSTALL_RPATH}")

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(mysqlrouter_bootstrap -lnsl -lsocket)
ENDIF()


INSTALL(TARGETS mysqlrouter_bootstrap
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

