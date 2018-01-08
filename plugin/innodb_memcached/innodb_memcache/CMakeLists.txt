# Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache
		    ${CMAKE_SOURCE_DIR}/storage/innobase/include
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/include
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/cache-src
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/include)

# If current CMAKE_C_FLAGS is with Werror, turn it off to compile successfully
IF(CMAKE_C_FLAGS MATCHES "-Werror")
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF(CMAKE_C_FLAGS MATCHES "-Werror")

# Set extra flags for the C/CXX compiler
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fno-strict-aliasing")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
ENDIF()

SET(MCD__UTILITITY_SOURCES
		util-src/config_parser.c
		util-src/util.c)

ADD_CONVENIENCE_LIBRARY(libmcd_util ${MCD__UTILITITY_SOURCES})

SET(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS}
                     -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/include
                     -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/cache-src")

SET(INNODB_ENGINE_SOURCES
		src/innodb_config.c
		src/innodb_utility.c
		src/hash_item_util.c
		src/innodb_engine.c
		src/innodb_api.c
		src/embedded_default_engine.c
		src/handler_api.cc
		cache-src/assoc.c
		cache-src/items.c
		cache-src/slabs.c)

# ADD_LIBRARY(innodb_engine SHARED ${INNODB_ENGINE_SOURCES})

MYSQL_ADD_PLUGIN(innodb_engine ${INNODB_ENGINE_SOURCES}
                 LINK_LIBRARIES libmcd_util
		 MODULE_ONLY MODULE_OUTPUT_NAME "innodb_engine")
