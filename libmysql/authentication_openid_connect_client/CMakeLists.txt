# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
# Configuration for building OpenID Connect authentication client Plug-in (client-side)
#

# The client authentication plug-in is part of the community build.

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping the OpenID Connect authentication client plugin.")
  RETURN()
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(
  authentication_openid_connect_client

  # Authentication plugin main
  authentication_openid_connect_client_plugin.cc

  LINK_LIBRARIES mysys OpenSSL::SSL OpenSSL::Crypto

  CLIENT_ONLY
  MODULE_ONLY MODULE_OUTPUT_NAME "authentication_openid_connect_client"
)

IF(LINUX OR SOLARIS)
  SET(PLUGIN_VERSION_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/authentication_openid_connect_client.ver)
  IF(SOLARIS)
    TARGET_LINK_OPTIONS(authentication_openid_connect_client PRIVATE
      LINKER:-z,gnu-version-script-compat)
  ENDIF()
  # hide all symbols in mysys, to avoid ODR violations.
  # There is *one* visible symbol: _mysql_client_plugin_declaration_
  TARGET_LINK_OPTIONS(authentication_openid_connect_client PRIVATE
    LINKER:--version-script=${PLUGIN_VERSION_FILE}
    )
  SET_TARGET_PROPERTIES(authentication_openid_connect_client
    PROPERTIES LINK_DEPENDS ${PLUGIN_VERSION_FILE})
ENDIF()
