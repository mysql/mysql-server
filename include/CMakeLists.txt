# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# These are the headers required to #include <mysql.h>, which we ship
# to the client as part of our API.
#
# FIXME: mysqld_error.h, which is also part of our API, should be included
# from mysql.h, but this needs more build system tweaking, since it's
# automatically generated.
SET(HEADERS
  errmsg.h
  mem_root_fwd.h
  my_command.h
  my_list.h
  mysql.h
  mysql_com.h
  mysql_time.h
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_version.h
  ${CMAKE_CURRENT_BINARY_DIR}/mysqld_error.h
  ../libbinlogevents/export/binary_log_types.h
)
SET(HEADERS_MYSQL_DIR
  mysql/client_plugin.h
  mysql/plugin_auth_common.h
  mysql/udf_registration_types.h
)

INSTALL(FILES ${HEADERS} DESTINATION ${INSTALL_INCLUDEDIR} COMPONENT Development)
INSTALL(FILES ${HEADERS_MYSQL_DIR} DESTINATION ${INSTALL_INCLUDEDIR}/mysql COMPONENT Development)
