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

DISABLE_MISSING_PROFILE_WARNING()

# Disable some options which are default ON:

SET(BUILD_TESTS OFF)
SET(BUILD_TESTS OFF CACHE INTERNAL "")

SET(BUILD_EXAMPLES OFF)
SET(BUILD_EXAMPLES OFF CACHE INTERNAL "")

SET(BUILD_MANPAGES OFF)
SET(BUILD_MANPAGES OFF CACHE INTERNAL "")

SET(FIDO_BUILD_SHARED_LIBS OFF)
SET(FIDO_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

SET(FIDO_BUILD_STATIC_LIBS OFF)
SET(FIDO_BUILD_STATIC_LIBS OFF CACHE INTERNAL "")

SET(BUILD_TOOLS OFF)
SET(BUILD_TOOLS OFF CACHE INTERNAL "")

IF(NOT LINUX)
  SET(NFC_LINUX OFF)
  SET(NFC_LINUX OFF CACHE INTERNAL "")
ENDIF()

# Use the same libraries as we use elsewhere.
SET(CBOR_LIBRARIES ext::cbor)
SET(ZLIB_LIBRARIES ext::zlib)
SET(CRYPTO_LIBRARIES OpenSSL::Crypto)

SET(FIDO_VERSION_DIR "libfido2-1.15.0")

SET(FIDO_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/${FIDO_VERSION_DIR}/src
  CACHE FILEPATH "Where to find header files for libfido2")

ADD_SUBDIRECTORY(${FIDO_VERSION_DIR})

ADD_LIBRARY(fido_interface INTERFACE)
TARGET_LINK_LIBRARIES(fido_interface INTERFACE fido2)
TARGET_INCLUDE_DIRECTORIES(fido_interface SYSTEM BEFORE INTERFACE
  ${FIDO_INCLUDE_DIR}
  )
ADD_LIBRARY(ext::fido ALIAS fido_interface)
