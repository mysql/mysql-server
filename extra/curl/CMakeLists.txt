# Copyright (c) 2022, 2023, Oracle and/or its affiliates.
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

SET(BUILD_CURL_EXE OFF)
SET(BUILD_CURL_EXE OFF CACHE INTERNAL "" FORCE)

SET(PICKY_COMPILER OFF)
SET(PICKY_COMPILER OFF CACHE INTERNAL "" FORCE)

SET(BUILD_SHARED_LIBS OFF)
SET(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)

SET(BUILD_STATIC_LIBS ON)
SET(BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)

SET(SHARE_LIB_OBJECT OFF)
SET(SHARE_LIB_OBJECT OFF CACHE INTERNAL "" FORCE)

SET(BUILD_TESTING OFF)
SET(BUILD_TESTING OFF CACHE INTERNAL "" FORCE)

SET(CURL_DISABLE_LDAP ON)
SET(CURL_DISABLE_LDAP ON CACHE INTERNAL "" FORCE)

SET(CURL_ENABLE_EXPORT_TARGET OFF)
SET(CURL_ENABLE_EXPORT_TARGET OFF CACHE INTERNAL "" FORCE)

SET(CURL_USE_LIBSSH2 OFF)
SET(CURL_USE_LIBSSH2 OFF CACHE INTERNAL "" FORCE)

SET(HTTP_ONLY ON)
SET(HTTP_ONLY ON CACHE INTERNAL "" FORCE)

SET(USE_LIBIDN2 OFF)
SET(USE_LIBIDN2 OFF CACHE INTERNAL "" FORCE)

SET(ENABLE_MANUAL OFF)
SET(ENABLE_MANUAL OFF CACHE INTERNAL "" FORCE)
SET(USE_MANUAL OFF)
SET(USE_MANUAL OFF CACHE INTERNAL "" FORCE)

SET(CURL_DISABLE_INSTALL ON)
SET(CURL_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)

SET(BUILD_LIBCURL_DOCS OFF)
SET(BUILD_LIBCURL_DOCS OFF CACHE INTERNAL "" FORCE)

IF(UNIX)
  STRING_APPEND(CMAKE_C_FLAGS   " -fvisibility=hidden")
  STRING_APPEND(CMAKE_CXX_FLAGS " -fvisibility=hidden")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("format-overflow" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS   " ${HAS_WARN_FLAG}")
ENDIF()

# Xcode >= 14 makes noise about sprintf.
IF(APPLE)
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-deprecated-declarations")
ENDIF()

MACRO(do_not_install)
ENDMACRO()

ADD_SUBDIRECTORY(${CURL_VERSION_DIR})
