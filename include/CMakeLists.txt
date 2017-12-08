# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

# These are the headers required to #include <mysql.h>, which we ship
# to the client as part of our API.
#
# FIXME: mysqld_error.h, which is also part of our API, should be included
# from mysql.h, but this needs more build system tweaking, since it's
# automatically generated.
SET(HEADERS
  errmsg.h
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
