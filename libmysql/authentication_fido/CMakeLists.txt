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
# Configuration for building FIDO Authentication Plugin (client-side)
#

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(STATUS "Skipping FIDO client authentication plugin.")
  RETURN()
ENDIF()

# The client authentication plugin is part of the community build.
# Build it if we have found the necessary libraries.
IF(NOT FIDO_FOUND)
  MESSAGE(WARNING "Skipping the FIDO client authentication plugin")
  RETURN()
ENDIF()

INCLUDE(CheckIncludeFiles)

MY_INCLUDE_SYSTEM_DIRECTORIES(FIDO)

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(authentication_fido_client
  fido_registration.cc
  fido_assertion.cc
  fido_client_plugin.cc
  LINK_LIBRARIES
  ${FIDO_LIBRARY}
  CLIENT_ONLY
  MODULE_ONLY
  MODULE_OUTPUT_NAME "authentication_fido_client")

IF (INSTALL_RPATH_FOR_FIDO2)
  ADD_INSTALL_RPATH_FOR_FIDO2(authentication_fido_client)
ENDIF()

IF(TARGET symlink_fido2_dlls)
  ADD_DEPENDENCIES(authentication_fido_client symlink_fido2_dlls)
ENDIF()
