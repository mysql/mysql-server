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
# Configuration for building Kerberos authentication client Plug-in (client-side)
#

# The client authentication plug-in is part of the community build.
# Build it if we have found the necessary libraries.

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping the KERBEROS client authentication plugin.")
  RETURN()
ENDIF()

IF(KERBEROS_LIB_SSPI)
  MESSAGE(STATUS "Building KERBEROS client authentication plug-in for windows.")
ELSE()
  MESSAGE(STATUS "Building KERBEROS client authentication plug-in for linux.")
ENDIF()

IF((KERBEROS_INCLUDE_DIR AND GSSAPI_INCLUDE_DIR))
  IF(NOT WITH_KERBEROS STREQUAL "system")
    IF(KERBEROS_INCLUDE_DIR)
      INCLUDE_DIRECTORIES(BEFORE SYSTEM ${KERBEROS_INCLUDE_DIR})
    ENDIF()
    IF(GSSAPI_INCLUDE_DIR)
      INCLUDE_DIRECTORIES(BEFORE SYSTEM ${GSSAPI_INCLUDE_DIR})
    ENDIF()
  ENDIF()
  SET(AUTH_CLIENT "gssapi_authentication_client.cc")
  SET(UTILITY "gssapi_utility.cc")
  SET(CORE "kerberos_core.cc")
  SET(KERBEROS_LIB "${GSSAPI_LIBRARIES}")
  LIST(APPEND KERBEROS_LIB "${KERBEROS_LIBRARIES}")
  MESSAGE(STATUS "Using Kerberos libraries: ${KERBEROS_LIB}")
ELSE()
  MESSAGE(WARNING
    "Skipping the KERBEROS client authentication plugin.")
  RETURN()
ENDIF()

IF(KERBEROS_LIB_SSPI)
  LIST(APPEND AUTH_CLIENT "sspi_authentication_client.cc")
  LIST(APPEND UTILITY "sspi_utility.cc")
ENDIF()

SET(MY_SQL "mysqlclient")

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(authentication_kerberos_client
  kerberos_client_interface.cc
  auth_kerberos_client_plugin.cc
  auth_kerberos_client_io.cc
  log_client.cc
  ${AUTH_CLIENT}
  ${CORE}
  ${UTILITY}
  LINK_LIBRARIES
  # Uncomment to verify that all symbols are found.
  # ${LINK_FLAG_NO_UNDEFINED}
  ${KERBEROS_LIB}
  ${SSL_LIBRARIES}
  ${LIBDL}
  ${MY_SQL}
  CLIENT_ONLY
  MODULE_ONLY
  MODULE_OUTPUT_NAME "authentication_kerberos_client")

IF(KERBEROS_LIB_SSPI)
  ADD_DEPENDENCIES(authentication_kerberos_client copy_kerberos_dlls)
ENDIF()

# MYSQL_ADD_PLUGIN may have decided not to build it.
IF(NOT TARGET authentication_kerberos_client)
  RETURN()
ENDIF()
