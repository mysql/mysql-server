# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

INCLUDE(GenerateExportHeader)

IF (NOT PROTOBUF_LITE_LIBRARY)
  MESSAGE(FATAL_ERROR "Routing plugin requires protobuf-lite library")
ENDIF()

ADD_LIBRARY(sql_lexer STATIC
  sql_lexer.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_lex_hash.cc
  )
TARGET_INCLUDE_DIRECTORIES(sql_lexer
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# generate sql_yacc.h
ADD_DEPENDENCIES(sql_lexer GenServerSource)

ADD_LIBRARY(routing SHARED
  mysql_routing.cc
  destination.cc
  dest_metadata_cache.cc
  dest_first_available.cc
  dest_next_available.cc
  dest_round_robin.cc
  routing.cc
  context.cc
  mysql_routing_common.cc
  connection_container.cc
  routing_component.cc
  channel.cc
  destination_ssl_context.cc
  classic_connection.cc
  x_connection.cc
  connection.cc
  blocked_endpoints.cc

  processor.cc
  classic_auth.cc
  classic_auth_cleartext.cc
  classic_auth_caching_sha2.cc
  classic_auth_forwarder.cc
  classic_auth_native.cc
  classic_auth_sha256_password.cc
  classic_binlog_dump.cc
  classic_change_user.cc
  classic_clone.cc
  classic_command.cc
  classic_connect.cc
  classic_flow.cc
  classic_forwarder.cc
  classic_frame.cc
  classic_greeting.cc
  classic_init_schema.cc
  classic_kill.cc
  classic_lazy_connect.cc
  classic_list_fields.cc
  classic_ping.cc
  classic_query.cc
  classic_quit.cc
  classic_register_replica.cc
  classic_reload.cc
  classic_reset_connection.cc
  classic_set_option.cc
  classic_statistics.cc
  classic_stmt_close.cc
  classic_stmt_execute.cc
  classic_stmt_fetch.cc
  classic_stmt_param_append_data.cc
  classic_stmt_prepare.cc
  classic_stmt_reset.cc

  sql_value.cc
)

TARGET_LINK_LIBRARIES(routing
  PUBLIC
  harness_stdx
  harness_tls
  mysql_protocol
  router_lib
  router_mysqlxmessages
  ${PROTOBUF_LITE_LIBRARY}
  metadata_cache
  connection_pool
  destination_status
  sql_lexer
  )
IF(SOLARIS)
  TARGET_LINK_LIBRARIES(routing
    PRIVATE -lnsl
    PRIVATE -lsocket)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(routing
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
SET_TARGET_PROPERTIES(routing PROPERTIES
  OUTPUT_NAME "mysqlrouter_routing"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
INSTALL(TARGETS routing
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )
ADD_DEPENDENCIES(routing GenLiteProtos)

ADD_INSTALL_RPATH_FOR_OPENSSL(routing)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(routing)

GENERATE_EXPORT_HEADER(routing
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/routing_export.h)
TARGET_COMPILE_DEFINITIONS(routing PRIVATE
  MYSQL_ROUTER_LOG_DOMAIN="routing")

# The Plugin
add_harness_plugin(routing_plugin
 OUTPUT_NAME "routing"
 SOURCES
 routing_plugin.cc
 plugin_config.cc
 REQUIRES routing
 DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
 )

GENERATE_EXPORT_HEADER(routing_plugin
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/routing_plugin_export.h)
