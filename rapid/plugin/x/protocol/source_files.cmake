# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

FILE(GLOB MYSQLX_PROTOBUF_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_datatypes.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_connection.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_expect.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_expr.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_crud.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_sql.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_session.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_notice.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/mysqlx_resultset.proto"
)

