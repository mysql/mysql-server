# Copyright (c) 2011, 2023, Oracle and/or its affiliates.
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

# Common defines and includes
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
  ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/include
  ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/cache-src
  ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/include
  )

# If current CMAKE_C_FLAGS is with Werror, turn it off to compile successfully
IF(CMAKE_C_FLAGS MATCHES "-Werror")
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF(CMAKE_C_FLAGS MATCHES "-Werror")

# Set extra flags for the C/CXX compiler
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING_APPEND(CMAKE_C_FLAGS " -std=gnu99 -fno-strict-aliasing")
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-parameter -Wno-cast-qual")
ENDIF()

SET(MCD__UTILITITY_SOURCES
  util-src/config_parser.c
  util-src/util.c)

ADD_LIBRARY(libmcd_util STATIC ${MCD__UTILITITY_SOURCES})

SET(INNODB_ENGINE_SOURCES
  src/innodb_config.cc
  src/innodb_utility.cc
  src/hash_item_util.cc
  src/innodb_engine.cc
  src/innodb_api.cc
  src/embedded_default_engine.c
  src/handler_api.cc
  cache-src/assoc.c
  cache-src/items.c
  cache-src/slabs.c)

MYSQL_ADD_PLUGIN(innodb_engine ${INNODB_ENGINE_SOURCES}
  LINK_LIBRARIES libmcd_util extra::rapidjson
  MODULE_ONLY MODULE_OUTPUT_NAME "innodb_engine")
