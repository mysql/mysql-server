# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 

MACRO (CHANGE_SSL_SETTINGS string)
  SET(WITH_SSL ${string} CACHE STRING "Options are : no, bundled, yes (prefer os library if present otherwise use bundled), system (use os library)" FORCE)
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_SSL)
  SET(INC_DIRS 
  ${CMAKE_SOURCE_DIR}/extra/yassl/include
  ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/include
  )
  SET(SSL_LIBRARIES  yassl taocrypt)
  SET(SSL_INCLUDE_DIRS ${INC_DIRS})
  SET(SSL_INTERNAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/mySTL)
  SET(SSL_DEFINES "-DHAVE_YASSL -DYASSL_PURE_C -DYASSL_PREFIX -DHAVE_OPENSSL -DYASSL_THREAD_SAFE")
  CHANGE_SSL_SETTINGS("bundled")
  #Remove -fno-implicit-templates 
  #(yassl sources cannot  be compiled with  it)
  SET(SAVE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  IF(CMAKE_CXX_FLAGS)
  STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS 
    ${CMAKE_CXX_FLAGS})
  ENDIF()
  ADD_SUBDIRECTORY(extra/yassl)
  ADD_SUBDIRECTORY(extra/yassl/taocrypt)
  SET(CMAKE_CXX_FLAGS ${SAVE_CXX_FLAGS})
  GET_TARGET_PROPERTY(src yassl SOURCES)
  FOREACH(file ${src})
    SET(SSL_SOURCES ${SSL_SOURCES} ${CMAKE_SOURCE_DIR}/extra/yassl/${file})
  ENDFOREACH()
  GET_TARGET_PROPERTY(src taocrypt SOURCES)
  FOREACH(file ${src})
    SET(SSL_SOURCES ${SSL_SOURCES} ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/${file})
  ENDFOREACH()
ENDMACRO()

# MYSQL_CHECK_SSL
#
# Provides the following configure options:
# WITH_SSL=[yes|no|bundled]
MACRO (MYSQL_CHECK_SSL)
  IF(NOT WITH_SSL)
   IF(WIN32)
     CHANGE_SSL_SETTINGS("bundled")
   ELSE()
     CHANGE_SSL_SETTINGS("no")
   ENDIF()
  ENDIF()

  IF(WITH_SSL STREQUAL "bundled")
    MYSQL_USE_BUNDLED_SSL()
  ELSEIF(WITH_SSL STREQUAL "system" OR WITH_SSL STREQUAL "yes")
    # Check for system library
    SET(OPENSSL_FIND_QUIETLY TRUE)
    INCLUDE(FindOpenSSL)
    FIND_LIBRARY(CRYPTO_LIBRARY crypto)
    MARK_AS_ADVANCED(CRYPTO_LIBRARY)
    INCLUDE(CheckSymbolExists)
    CHECK_SYMBOL_EXISTS(SHA512_DIGEST_LENGTH "openssl/sha.h" 
                        HAVE_SHA512_DIGEST_LENGTH)
    IF(OPENSSL_FOUND AND CRYPTO_LIBRARY AND HAVE_SHA512_DIGEST_LENGTH)
      SET(SSL_SOURCES "")
      SET(SSL_LIBRARIES ${OPENSSL_LIBRARIES} ${CRYPTO_LIBRARY})
      SET(SSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
      SET(SSL_INTERNAL_INCLUDE_DIRS "")
      SET(SSL_DEFINES "-DHAVE_OPENSSL")
      CHANGE_SSL_SETTINGS("system")
    ELSE()
      IF(WITH_SSL STREQUAL "system")
        MESSAGE(SEND_ERROR "Cannot find appropriate system libraries for SSL. Use  WITH_SSL=bundled to enable SSL support")
      ENDIF()
      MYSQL_USE_BUNDLED_SSL()
    ENDIF()
  ELSEIF(NOT WITH_SSL STREQUAL "no")
    MESSAGE(SEND_ERROR "Wrong option for WITH_SSL. Valid values are : yes, no, bundled")
  ENDIF()
ENDMACRO()
