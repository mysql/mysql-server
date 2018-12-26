# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

SET(MYSQLX_PLUGIN_VERSION_MAJOR 1)
SET(MYSQLX_PLUGIN_VERSION_MINOR 0)
SET(MYSQLX_PLUGIN_VERSION_PATCH 2)

SET(MYSQLX_PLUGIN_NAME "mysqlx")

STRING(SUBSTRING ${MYSQLX_PLUGIN_NAME} 0 1 MYSQLX_PLUGIN_NAME_FIRST_LETTER)
STRING(SUBSTRING ${MYSQLX_PLUGIN_NAME} 1 -1 MYSQLX_PLUGIN_NAME_REST)
STRING(TOUPPER ${MYSQLX_PLUGIN_NAME_FIRST_LETTER} MYSQLX_PLUGIN_NAME_FIRST_LETTER)

SET(MYSQLX_STATUS_VARIABLE_NAME "${MYSQLX_PLUGIN_NAME_FIRST_LETTER}${MYSQLX_PLUGIN_NAME_REST}")
SET(MYSQLX_SYSTEM_VARIABLE_NAME "${MYSQLX_PLUGIN_NAME}")

IF(NOT MYSQLX_TCP_PORT)
  SET(MYSQLX_TCP_PORT 33060)
ENDIF(NOT MYSQLX_TCP_PORT)

IF(NOT MYSQLX_UNIX_ADDR)
  GET_FILENAME_COMPONENT(DIR_OF_UNIX_ADDR "${MYSQL_UNIX_ADDR}" PATH)
  SET(MYSQLX_UNIX_ADDR "${DIR_OF_UNIX_ADDR}/mysqlx.sock")
ENDIF(NOT MYSQLX_UNIX_ADDR)
