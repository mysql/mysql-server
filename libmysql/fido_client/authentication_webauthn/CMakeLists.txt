# Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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
# Configuration for building FIDO Authentication Plugin (client-side)
#

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping webauthn client authentication plugin.")
  RETURN()
ENDIF()

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="authentication_webauthn")
# The client authentication plugin is part of the community build.
# Build it if we have found the necessary libraries.
IF(NOT FIDO_FOUND)
  MESSAGE(WARNING "Skipping the webauthn client authentication plugin")
  RETURN()
ENDIF()

INCLUDE(CheckIncludeFiles)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  )

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(authentication_webauthn_client
  webauthn_registration.cc
  webauthn_assertion.cc
  webauthn_client_plugin.cc
  LINK_LIBRARIES
  fido_client_common
  ext::fido
  OpenSSL::Crypto  # Needs RAND_bytes from libcrypto.
  CLIENT_ONLY
  MODULE_ONLY
  MODULE_OUTPUT_NAME "authentication_webauthn_client")

IF (INSTALL_RPATH_FOR_FIDO2)
  ADD_INSTALL_RPATH_FOR_FIDO2(authentication_webauthn_client)
ENDIF()

IF(TARGET symlink_fido2_dlls)
  ADD_DEPENDENCIES(authentication_webauthn_client symlink_fido2_dlls)
ENDIF()
