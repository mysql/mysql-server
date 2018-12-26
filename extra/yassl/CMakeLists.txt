# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/extra/yassl/include
 ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/include
 ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/mySTL)

ADD_DEFINITIONS(${SSL_DEFINES})

# rename get_tty_password to avoid collisions with the main binary
ADD_DEFINITIONS(-Dget_tty_password_ext=yassl_mysql_get_tty_password_ext)
ADD_DEFINITIONS(-Dget_tty_password=yassl_mysql_get_tty_password)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

MY_CHECK_CXX_COMPILER_FLAG("-Wstringop-overflow" HAVE_STRINGOP_OVERFLOW)
IF(HAVE_STRINGOP_OVERFLOW)
  ADD_COMPILE_FLAGS(
    src/ssl.cpp
    src/yassl_imp.cpp
    COMPILE_FLAGS -Wno-stringop-overflow
    )
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-const-variable" HAVE_NO_UNUSED_CONST_VAR)

IF(CMAKE_GENERATOR MATCHES "Xcode" AND HAVE_NO_UNUSED_CONST_VAR)
  ADD_COMPILE_FLAGS(
    src/yassl_int.cpp
    src/socket_wrapper.cpp
    COMPILE_FLAGS -Wno-unused-const-variable)
ENDIF()

SET(YASSL_SOURCES  src/buffer.cpp src/cert_wrapper.cpp src/crypto_wrapper.cpp src/handshake.cpp src/lock.cpp 
				src/log.cpp src/socket_wrapper.cpp src/ssl.cpp src/timer.cpp src/yassl_error.cpp 
				src/yassl_imp.cpp src/yassl_int.cpp
                                ../../client/get_password.c )

ADD_CONVENIENCE_LIBRARY(yassl ${YASSL_SOURCES})
