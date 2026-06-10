# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping the NATIVE_PASSWORD client authentication plugin.")
  RETURN()
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

MESSAGE(STATUS "building mysql_native_password client shared plugin")

MYSQL_ADD_PLUGIN(mysql_native_password
  mysql_native_password.cc
  LINK_LIBRARIES mysys
  MODULE_ONLY
  CLIENT_ONLY
  MODULE_OUTPUT_NAME "mysql_native_password"
)

IF(LINUX)
  SET(PLUGIN_VERSION_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_native_password.ver)
  # Hide all symbols in mysys, to avoid ODR violations.
  # There is *one* visible symbol: _mysql_client_plugin_declaration_
  TARGET_LINK_OPTIONS(mysql_native_password PRIVATE
    LINKER:--version-script=${PLUGIN_VERSION_FILE}
    )
ENDIF()
