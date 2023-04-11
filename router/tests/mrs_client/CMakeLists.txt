# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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
  client/basic_authentication.cc
  configuration/url.cc
  COMPONENT Test
  LINK_LIBRARIES
  http_client
  harness-library
  ${SSL_LIBRARIES}
  DEPENDENCIES
  mysql_rest_service_if
  )

TARGET_INCLUDE_DIRECTORIES(mysqlrouter_mrs_client
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

TARGET_LINK_LIBRARIES(mysqlrouter_mrs_client mysql_rest_service_if)

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)

