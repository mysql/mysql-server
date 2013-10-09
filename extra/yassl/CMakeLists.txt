# Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
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
IF(CMAKE_COMPILER_IS_GNUXX)
  #Remove -fno-implicit-templates 
  #(yassl sources cannot  be compiled with  it)
  STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS
${CMAKE_CXX_FLAGS})
ENDIF()
SET(YASSL_SOURCES  src/buffer.cpp src/cert_wrapper.cpp src/crypto_wrapper.cpp src/handshake.cpp src/lock.cpp 
				src/log.cpp src/socket_wrapper.cpp src/ssl.cpp src/timer.cpp src/yassl_error.cpp 
				src/yassl_imp.cpp src/yassl_int.cpp)

IF(HAVE_EXPLICIT_TEMPLATE_INSTANTIATION)
  SET(YASSL_SOURCES ${YASSL_SOURCES} src/template_instnt.cpp)
ENDIF()

ADD_CONVENIENCE_LIBRARY(yassl ${YASSL_SOURCES})
RESTRICT_SYMBOL_EXPORTS(yassl)

INSTALL_DEBUG_SYMBOLS(yassl)
IF(MSVC)
   INSTALL_DEBUG_TARGET(yassl DESTINATION ${INSTALL_LIBDIR}/debug)
ENDIF()


