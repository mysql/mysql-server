# Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

#
# Configuration for building OCI authentication client Plug-in (client-side)
#

# The client authentication plug-in is part of the community build.

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping the OCI authentication client plugin.")
  RETURN()
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(
  authentication_oci_client

  # Authentication plugin main
  authentication_oci_client_plugin.cc

  # Request signing
  ${CMAKE_SOURCE_DIR}/sql-common/oci/ssl.cc
  ${CMAKE_SOURCE_DIR}/sql-common/oci/signing_key.cc

  # Utilities
  ${CMAKE_SOURCE_DIR}/sql-common/oci/utilities.cc

  LINK_LIBRARIES ${SSL_LIBRARIES}

  CLIENT_ONLY
  MODULE_ONLY MODULE_OUTPUT_NAME "authentication_oci_client"
)

