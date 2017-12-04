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

IF(WITHOUT_SERVER)
  MESSAGE(STATUS "The mysqlservices library is for server plugins only")
  RETURN()
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(MYSQLSERVICES_SOURCES
  service_command.c
  locking_service.c
  srv_session_service.c
  my_snprintf_service.c
  thd_alloc_service.c
  thd_wait_service.c
  my_plugin_log_service.c
  my_thread_scheduler_service.c
  mysql_string_service.c
  mysql_malloc_service.c
  mysql_password_policy_service.c
  parser_service.c
  srv_session_info_service.c
  rpl_transaction_ctx_service.c
  rpl_transaction_write_set_service.c
  security_context_service.c
  mysql_keyring_service.c)

ADD_CONVENIENCE_LIBRARY(mysqlservices ${MYSQLSERVICES_SOURCES})
INSTALL(TARGETS mysqlservices DESTINATION ${INSTALL_LIBDIR} COMPONENT Development)
