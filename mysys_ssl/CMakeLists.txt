# Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/mysys_ssl
                    ${CMAKE_SOURCE_DIR}/mysys
                    ${SSL_INCLUDE_DIRS})

IF(SSL_DEFINES)
ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

SET(MYSYS_AES_IMPLEMENTATION my_aes_openssl.cc)

SET(MYSYS_SSL_SOURCES
    crypt_genhash_impl.cc
    mf_tempdir.cc
    my_default.cc
    my_getopt.cc
    my_aes.cc
    my_sha1.cc
    my_md5.cc
    my_rnd.cc
    my_murmur3.cc
    ${MYSYS_AES_IMPLEMENTATION}
   )

ADD_CONVENIENCE_LIBRARY(mysys_ssl ${MYSYS_SSL_SOURCES})
TARGET_LINK_LIBRARIES(mysys_ssl dbug strings ${SSL_LIBRARIES} ${ZLIB_LIBRARY})
TARGET_LINK_LIBRARIES(mysys_ssl ${CMAKE_THREAD_LIBS_INIT})
# Needed to resolve OpenSSL dependency on Windows
IF(WIN32)
  TARGET_LINK_LIBRARIES(mysys_ssl crypt32)
ENDIF()
DTRACE_INSTRUMENT(mysys_ssl)

# MD5_Init() MD5_Update() MD5_Final() are all deprecated
IF(SOLARIS AND CMAKE_COMPILER_IS_GNUCXX)
  ADD_COMPILE_FLAGS(my_md5.cc COMPILE_FLAGS "-Wno-deprecated-declarations")
ENDIF()
