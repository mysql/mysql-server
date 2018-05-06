# Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
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

IF(SSL_DEFINES)
ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(WITH_SSL STREQUAL "wolfssl")
  SET(MYSYS_AES_IMPLEMENTATION my_aes_wolfssl.cc)
ELSE()
  SET(MYSYS_AES_IMPLEMENTATION my_aes_openssl.cc)
ENDIF()

SET(MYSYS_SSL_SOURCES
    crypt_genhash_impl.cc
    my_default.cc
    my_getopt.cc
    my_aes.cc
    my_sha1.cc
    my_sha2.cc
    my_md5.cc
    my_rnd.cc
    ${MYSYS_AES_IMPLEMENTATION}
   )

ADD_CONVENIENCE_LIBRARY(mysys_ssl ${MYSYS_SSL_SOURCES})
TARGET_LINK_LIBRARIES(mysys_ssl dbug strings ${SSL_LIBRARIES} ${ZLIB_LIBRARY})
# Needed to resolve OpenSSL dependency on Windows
IF(WIN32 AND NOT WITH_SSL STREQUAL "wolfssl")
  TARGET_LINK_LIBRARIES(mysys_ssl crypt32)
ENDIF()
