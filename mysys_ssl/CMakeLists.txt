# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/mysys_ssl
                    ${CMAKE_SOURCE_DIR}/mysys
                    ${SSL_INCLUDE_DIRS})

IF(SSL_DEFINES)
ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(WITH_SSL STREQUAL "bundled")
  SET(MYSYS_AES_IMPLEMENTATION my_aes_yassl.cc)
ELSE()
  SET(MYSYS_AES_IMPLEMENTATION my_aes_openssl.cc)
ENDIF()

SET(MYSYS_SSL_SOURCES
    crypt_genhash_impl.cc
    mf_tempdir.cc
    my_default.cc
    my_getopt.cc
    my_aes.cc
    my_sha1.cc
    my_sha2.cc
    my_md5.cc
    my_rnd.cc
    my_murmur3.cc
    ${MYSYS_AES_IMPLEMENTATION}
   )

ADD_CONVENIENCE_LIBRARY(mysys_ssl ${MYSYS_SSL_SOURCES})
TARGET_LINK_LIBRARIES(mysys_ssl dbug strings ${SSL_LIBRARIES} ${ZLIB_LIBRARY})
# Needed to resolve OpenSSL dependency on Windows
IF(WIN32 AND NOT WITH_SSL STREQUAL "bundled")
  TARGET_LINK_LIBRARIES(mysys_ssl crypt32)
ENDIF()
DTRACE_INSTRUMENT(mysys_ssl)
