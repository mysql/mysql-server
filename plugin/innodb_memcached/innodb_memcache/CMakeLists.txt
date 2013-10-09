# Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_SHARED_LIBRARY_C_FLAGS} -std=gnu99")

SET(MCD__UTILITITY_SOURCES
		util-src/config_parser.c
		util-src/util.c)

ADD_LIBRARY(libmcd_util STATIC ${MCD__UTILITITY_SOURCES})

SET(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS}
                     -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/include
                     -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/cache-src")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_SHARED_LIBRARY_C_FLAGS} -std=gnu99")

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
		MODULE_ONLY MODULE_OUTPUT_NAME "innodb_engine")

TARGET_LINK_LIBRARIES(innodb_engine libmcd_util)

