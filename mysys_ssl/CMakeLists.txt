# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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
                    ${SSL_INCLUDE_DIRS})

IF(SSL_DEFINES)
ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

# We do RESTRICT_SYMBOL_EXPORTS(yassl) elsewhere.
# In order to get correct symbol visibility, these files
# must be compiled with "-fvisibility=hidden"
IF(WITH_SSL STREQUAL "bundled" AND HAVE_VISIBILITY_HIDDEN)
  SET_SOURCE_FILES_PROPERTIES(
    crypt_genhash_impl.cc
    my_aes.cc
    my_md5.cc
    my_sha1.cc
    my_sha2.cc
    PROPERTIES COMPILE_FLAGS "-fvisibility=hidden")
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
    my_murmur3.cc
   )

ADD_CONVENIENCE_LIBRARY(mysys_ssl ${MYSYS_SSL_SOURCES})
TARGET_LINK_LIBRARIES(mysys_ssl dbug strings ${SSL_LIBRARIES} ${ZLIB_LIBRARY})
DTRACE_INSTRUMENT(mysys_ssl)

INSTALL_DEBUG_SYMBOLS(mysys_ssl)
IF(MSVC)
   INSTALL_DEBUG_TARGET(mysys_ssl DESTINATION ${INSTALL_LIBDIR}/debug)
ENDIF()

