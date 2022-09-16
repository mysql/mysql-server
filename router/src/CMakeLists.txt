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

ADD_SUBDIRECTORY(connection_pool)
ADD_SUBDIRECTORY(destination_status)
ADD_SUBDIRECTORY(harness)
ADD_SUBDIRECTORY(http)
ADD_SUBDIRECTORY(io)
ADD_SUBDIRECTORY(json_schema_embedder)
ADD_SUBDIRECTORY(keepalive)
ADD_SUBDIRECTORY(metadata_cache)
ADD_SUBDIRECTORY(mock_server)
ADD_SUBDIRECTORY(mysql_protocol)
ADD_SUBDIRECTORY(mysqlxmessages)
ADD_SUBDIRECTORY(openssl)
ADD_SUBDIRECTORY(plugin_info)
ADD_SUBDIRECTORY(protobuf)
ADD_SUBDIRECTORY(rest_api)
ADD_SUBDIRECTORY(rest_connection_pool)
ADD_SUBDIRECTORY(rest_metadata_cache)
ADD_SUBDIRECTORY(rest_router)
ADD_SUBDIRECTORY(rest_routing)
ADD_SUBDIRECTORY(router)
ADD_SUBDIRECTORY(routing)

# Directory layout after 'make install' is different.
# Create symlinks to ssl libraries for both: build and install layouts
IF (APPLE_WITH_CUSTOM_SSL)
  FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter")

  ADD_CUSTOM_TARGET(link_openssl_dlls_for_install_mysqlrouter ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "../../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "../../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter"

    BYPRODUCTS
    "${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter/${CRYPTO_VERSION}"
    "${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter/${OPENSSL_VERSION}"
  )

  ADD_DEPENDENCIES(mysqlrouter_all
    link_openssl_dlls_for_install_mysqlrouter)
  ADD_DEPENDENCIES(mysqlrouter_all
    link_openssl_dlls)

  INSTALL(FILES
    ${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter/${CRYPTO_VERSION}
    ${CMAKE_BINARY_DIR}/plugin_output_directory/mysqlrouter/${OPENSSL_VERSION}
    DESTINATION ${ROUTER_INSTALL_PLUGINDIR} COMPONENT Router
  )
ENDIF()
