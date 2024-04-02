# Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

MYSQL_ADD_EXECUTABLE(mysqlrouter_mrs_client
  main.cc
  client/http_client_request.cc
  client/session.cc
  client/authentication.cc
  COMPONENT Test
  LINK_LIBRARIES
  PUBLIC http_client
  PUBLIC harness-library
  PUBLIC harness_tls
  PUBLIC router_openssl
  PUBLIC mysql_rest_service_if
  ${SSL_LIBRARIES}
  DEPENDENCIES
  mysql_rest_service_if
  )

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
