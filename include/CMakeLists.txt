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
${CMAKE_CURRENT_BINARY_DIR}/mysql_version.h
${CMAKE_CURRENT_BINARY_DIR}/my_config.h
${CMAKE_CURRENT_BINARY_DIR}/mysqld_ername.h
${CMAKE_CURRENT_BINARY_DIR}/mysqld_error.h
${CMAKE_CURRENT_BINARY_DIR}/sql_state.h
)
SET(HEADERS_ABI 
  mysql.h 
  mysql_com.h
  mysql_time.h 
  my_list.h
  my_alloc.h
  typelib.h 
  mysql/plugin.h
  mysql/plugin_audit.h
  mysql/plugin_ftparser.h
)

SET(HEADERS 
  ${HEADERS_ABI} 
  my_dbug.h 
  m_string.h
  my_sys.h 
  my_xml.h
  mysql_embed.h
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
  sql_common.h
  keycache.h
  m_ctype.h
  my_attribute.h 
  my_compiler.h
  ${HEADERS_GEN_CONFIGURE}
)

INSTALL(FILES ${HEADERS} DESTINATION ${INSTALL_INCLUDEDIR} COMPONENT Development)
INSTALL(DIRECTORY mysql/ DESTINATION ${INSTALL_INCLUDEDIR}/mysql COMPONENT Development FILES_MATCHING PATTERN "*.h")
