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

# Skip it if disabled.
IF(NOT WITH_AUTHENTICATION_CLIENT_PLUGINS)
  MESSAGE(WARNING "Skipping the fido_client_common library.")
  RETURN()
ENDIF()

# The client authentication plugin is part of the community build.
# Build it if we have found the necessary libraries.
IF(NOT FIDO_FOUND)
  MESSAGE(WARNING "Skipping the fido_client_common library.")
  RETURN()
ENDIF()

ADD_WSHADOW_WARNING()
SET(FIDO_COMMON_SOURCES
   assertion.cc
   common.cc
   registration.cc
   )

ADD_CONVENIENCE_LIBRARY(
  fido_client_common
  ${FIDO_COMMON_SOURCES}
  LINK_LIBRARIES
  mysqlclient
  ext::fido
)
