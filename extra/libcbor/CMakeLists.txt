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

SET(CBOR_VERSION "0.11.0")

DISABLE_MISSING_PROFILE_WARNING()

# not all control paths return a value
IF(WIN32)
  STRING_APPEND(CMAKE_C_FLAGS " /wd4715")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmaybe-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wdocumentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wstrict-prototypes" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wreturn-type" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# Disable some options which are default ON:

SET(CBOR_PRETTY_PRINTER OFF)
SET(CBOR_PRETTY_PRINTER OFF CACHE INTERNAL "")

SET(WITH_EXAMPLES OFF)
SET(WITH_EXAMPLES OFF CACHE INTERNAL "")

SET(SANITIZE OFF)
SET(SANITIZE OFF CACHE INTERNAL "")

MACRO(do_not_install)
ENDMACRO()

SET(CBOR_VERSION_DIR "libcbor-0.11.0")

ADD_SUBDIRECTORY(${CBOR_VERSION_DIR})

ADD_LIBRARY(cbor_interface INTERFACE)
TARGET_LINK_LIBRARIES(cbor_interface INTERFACE cbor)
TARGET_INCLUDE_DIRECTORIES(cbor_interface SYSTEM BEFORE INTERFACE
  ${CMAKE_CURRENT_BINARY_DIR}/${CBOR_VERSION_DIR}/
  ${CMAKE_CURRENT_SOURCE_DIR}/${CBOR_VERSION_DIR}/src
  )
ADD_LIBRARY(ext::cbor ALIAS cbor_interface)
