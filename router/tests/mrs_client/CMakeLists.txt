# Copyright (c) 2017, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Embed the Router libraries as the MRS component does.
SET(MRS_CLIENT_LIBRARIES
  http_client_object
  http_common_object
  harness-library_object
  harness_stdx_object
  harness_tls_object
  router_utils_object
  mysqlclient
  router_mysqlclient_shim
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT})
IF(WIN32)
  LIST(APPEND MRS_CLIENT_LIBRARIES shlwapi.dll)
ELSEIF(SOLARIS)
  LIST(APPEND MRS_CLIENT_LIBRARIES nsl socket)
ENDIF()

MYSQL_ADD_EXECUTABLE(mysqlrouter_mrs_client
  main.cc
  client/http_client_request.cc
  client/session.cc
  client/authentication.cc
  COMPONENT Router
  LINK_LIBRARIES
  router_tls
  mysql_rest_service_if
  mysql_rest_service_helper
  ${MRS_CLIENT_LIBRARIES}
  ${SSL_LIBRARIES}
  DEPENDENCIES
  mysql_rest_service_if
  )

TARGET_COMPILE_DEFINITIONS(mysqlrouter_mrs_client PRIVATE
  HTTP_CLIENT_STATIC_DEFINE
  HTTP_COMMON_STATIC_DEFINE
  HARNESS_STATIC_DEFINE
  HARNESS_STDX_STATIC_DEFINE
  HARNESS_TLS_STATIC_DEFINE
  ROUTER_UTILS_STATIC_DEFINE)
IF(WIN32)
  # Router objects may import symbols resolved within the same executable.
  TARGET_LINK_OPTIONS(mysqlrouter_mrs_client PRIVATE "/IGNORE:4217")
ENDIF()

# mysqlrouter_mrs_client component is set to Test, which
# do not adds it to `mysqlrouter_all`.
ADD_DEPENDENCIES(mysqlrouter_all mysqlrouter_mrs_client)

TARGET_INCLUDE_DIRECTORIES(mysqlrouter_mrs_client
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # TODO(areliga): had to add it while rebasing to 9.0
    # this should likely be solved some other way
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/http/src/
    )
