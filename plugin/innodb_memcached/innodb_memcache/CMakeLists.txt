# Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
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

# Common defines and includes
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache
		    ${CMAKE_SOURCE_DIR}/include
		    ${CMAKE_SOURCE_DIR}/sql
		    ${CMAKE_SOURCE_DIR}/bld/include
		    ${CMAKE_SOURCE_DIR}/regex
		    ${CMAKE_SOURCE_DIR}/storage/innobase/include
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/include
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/cache-src
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/include)

# If current CMAKE_C_FLAGS is with Werror, turn it off to compile successfully
IF(CMAKE_C_FLAGS MATCHES "-Werror")
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  # Turn -Wdeclaration-after-statement off to reduce the number of warnings
  STRING(REGEX REPLACE "-Wdeclaration-after-statement" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF(CMAKE_C_FLAGS MATCHES "-Werror")

# Set extra flags for the C compiler
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} ${CMAKE_SHARED_LIBRARY_C_FLAGS} -std=gnu99")
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
