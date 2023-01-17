# Copyright (c) 2022, 2023, Oracle and/or its affiliates.
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


INCLUDE_DIRECTORIES(BEFORE SYSTEM ${ZSTD_INCLUDE_DIR})
LINK_DIRECTORIES(BEFORE ${OPENSSL_LIB_DIR})

SET(BUILD_CURL_EXE OFF)
SET(BUILD_CURL_EXE OFF CACHE INTERNAL "" FORCE)

SET(BUILD_SHARED_LIBS OFF)

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


STRING_APPEND(CMAKE_C_FLAGS   " -fvisibility=hidden")
STRING_APPEND(CMAKE_CXX_FLAGS " -fvisibility=hidden")

MACRO(do_not_install)
ENDMACRO()

ADD_SUBDIRECTORY(${CURL_VERSION_DIR})
