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

SET(MYSQLX_PROTOBUF_INCLUDE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

FILE(GLOB MYSQLX_PROTOBUF_FILES
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_datatypes.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_connection.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_expect.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_expr.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_crud.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_sql.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_session.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_notice.proto"
  "${MYSQLX_PROTOBUF_INCLUDE_DIR}/mysqlx_resultset.proto"
)

