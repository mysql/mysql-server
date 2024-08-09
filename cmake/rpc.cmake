# Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

IF(WIN32)
  RETURN()
ENDIF()

FUNCTION(WARN_MISSING_RPCGEN_EXECUTABLE)
  IF(NOT RPCGEN_EXECUTABLE)
    MESSAGE(WARNING "Cannot find rpcgen executable. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install rpcsvc-proto\n"
      "  RedHat/Fedora/Oracle Linux: yum install rpcgen\n"
      "  SuSE:                       zypper install glibc-devel\n"
      )
  ENDIF()
ENDFUNCTION()

FUNCTION(WARN_MISSING_SYSTEM_TIRPC)
  IF(NOT RPC_INCLUDE_DIRS)
    MESSAGE(WARNING "Cannot find RPC development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libtirpc-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libtirpc-devel\n"
      "  SuSE:                       zypper install glibc-devel\n"
      )
  ENDIF()
ENDFUNCTION()

FUNCTION(MYSQL_CHECK_RPC)
  IF(WITH_TIRPC STREQUAL "bundled")
    SET(WITH_TIRPC "bundled" CACHE INTERNAL "")
    SET(TIRPC_FOUND TRUE)

    SET(TIRPC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/tirpc/include/tirpc")
    SET(RPC_INCLUDE_DIRS "${TIRPC_INCLUDE_DIR}")
    SET(TIRPC_VERSION "1.3.5")

    ADD_SUBDIRECTORY(extra/tirpc)

  ELSEIF(NOT LIBTIRPC_VERSION_TOO_OLD)
    MYSQL_CHECK_PKGCONFIG()
    PKG_CHECK_MODULES(TIRPC libtirpc)
  ENDIF()

  IF(TIRPC_FOUND)
    IF(TIRPC_VERSION VERSION_LESS 1.0)
      SET(LIBTIRPC_VERSION_TOO_OLD 1 CACHE INTERNAL "libtirpc is too old" FORCE)
      MESSAGE(WARNING
        "Ignoring libtirpc version ${TIRPC_VERSION}, need at least 1.0")
      UNSET(TIRPC_FOUND)
      UNSET(TIRPC_FOUND CACHE)
      UNSET(pkgcfg_lib_TIRPC_tirpc)
      UNSET(pkgcfg_lib_TIRPC_tirpc CACHE)
      GET_CMAKE_PROPERTY(CACHE_VARS CACHE_VARIABLES)
      FOREACH(CACHE_VAR ${CACHE_VARS})
        IF(CACHE_VAR MATCHES "^TIRPC_.*")
          UNSET(${CACHE_VAR})
          UNSET(${CACHE_VAR} CACHE)
        ENDIF()
      ENDFOREACH()
    ENDIF()
  ENDIF()

  IF(TIRPC_FOUND AND NOT WITH_TIRPC STREQUAL "bundled")
    SET(WITH_TIRPC "system" CACHE INTERNAL "")
    ADD_LIBRARY(ext::rpc SHARED IMPORTED GLOBAL)
    SET_TARGET_PROPERTIES(ext::rpc PROPERTIES
      IMPORTED_LOCATION ${pkgcfg_lib_TIRPC_tirpc}
      INTERFACE_COMPILE_DEFINITIONS HAVE_SYSTEM_TIRPC
      )
    TARGET_INCLUDE_DIRECTORIES(ext::rpc
      SYSTEM BEFORE INTERFACE "${TIRPC_INCLUDE_DIRS}"
      )

    # RPC headers may be found in /usr/include rather than /usr/include/tirpc
    IF(TIRPC_INCLUDE_DIRS)
      SET(RPC_INCLUDE_DIRS ${TIRPC_INCLUDE_DIRS})
    ELSE()
      FIND_PATH(RPC_INCLUDE_DIRS NAMES rpc/rpc.h)
    ENDIF()
  ELSE()
    FIND_PATH(RPC_INCLUDE_DIRS NAMES rpc/rpc.h)
  ENDIF()

  IF(NOT RPC_INCLUDE_DIRS)
    WARN_MISSING_SYSTEM_TIRPC()
    MESSAGE(FATAL_ERROR
      "Could not find rpc/rpc.h in /usr/include or /usr/include/tirpc")
  ENDIF()
  SET(RPC_INCLUDE_DIRS "${RPC_INCLUDE_DIRS}" PARENT_SCOPE)

ENDFUNCTION(MYSQL_CHECK_RPC)
