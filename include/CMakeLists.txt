# Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.
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

SET(HEADERS_GEN_CONFIGURE 
  mysql_version.h
  my_config.h
  mysqld_ername.h
  mysqld_error.h
  sql_state.h
)

SET(HEADERS 
  mysql.h 
  mysql_com.h
  mysql_time.h 
  ma_dyncol.h
  my_list.h
  my_alloc.h
  typelib.h 
  my_dbug.h 
  m_string.h
  my_sys.h 
  my_xml.h
  mysql_embed.h
  my_decimal_limits.h
  my_pthread.h
  decimal.h
  errmsg.h
  my_global.h
  my_net.h 
  my_getopt.h
  sslopt-longopts.h
  my_dir.h 
  sslopt-vars.h
  sslopt-case.h
  my_valgrind.h
  sql_common.h
  keycache.h
  m_ctype.h
  my_attribute.h 
  my_compiler.h
  handler_state.h
  handler_ername.h
)

INSTALL(FILES ${HEADERS} DESTINATION ${INSTALL_INCLUDEDIR} COMPONENT Development)
FOREACH(f ${HEADERS_GEN_CONFIGURE})
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${f} DESTINATION ${INSTALL_INCLUDEDIR} COMPONENT Development PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
ENDFOREACH(f)
INSTALL(DIRECTORY mysql/ DESTINATION ${INSTALL_INCLUDEDIR} COMPONENT Development FILES_MATCHING PATTERN "*.h")

STRING(REPLACE "." "\\." EXCL_RE "${HEADERS};${HEADERS_GEN_CONFIGURE}")
STRING(REPLACE ";" "|" EXCL_RE "${EXCL_RE}")

INSTALL(DIRECTORY . DESTINATION ${INSTALL_INCLUDEDIR}/private COMPONENT Development
  FILES_MATCHING PATTERN "*.h"
  PATTERN CMakeFiles EXCLUDE
  PATTERN mysql EXCLUDE
  REGEX "\\./(${EXCL_RE}$)" EXCLUDE)
