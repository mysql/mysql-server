# Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/mysql
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/include
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/include/memcached
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached

		)
INCLUDE_DIRECTORIES(SYSTEM ${LIBEVENT_INCLUDE_DIRS})

SET(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/utilities")

# If current CMAKE_C_FLAGS is with Werror, turn it off to compile successfully
IF(CMAKE_C_FLAGS MATCHES "-Werror")
  STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDIF(CMAKE_C_FLAGS MATCHES "-Werror")

# Set extra flags for the C compiler
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
ENDIF()


SET(LIBMEMCACHED_UTILITIES_SOURCES
		include/memcached/config_parser.h
		include/memcached/genhash.h
		include/memcached/util.h
		utilities/config_parser.c
		utilities/engine_loader.c
		utilities/engine_loader.h
		utilities/extension_loggers.c
		utilities/genhash.c
		utilities/genhash_int.h
		utilities/util.c)

ADD_CONVENIENCE_LIBRARY(memcached_utilities ${LIBMEMCACHED_UTILITIES_SOURCES})

SET(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/daemon_memcached/daemon")

SET(MEMCACHED_SOURCES
		daemon/memcached_mysql.h
		daemon/memcached_mysql.cc
		daemon/cache.h
		daemon/cache.c
		config_static.h
		daemon/daemon.c
		daemon/hash.c
		daemon/hash.h
		daemon/memcached.c
		daemon/memcached.h
		daemon/sasl_defs.c
		daemon/sasl_defs.h
		daemon/stats.c
		daemon/stats.h
		daemon/thread.c
		daemon/topkeys.c
		daemon/topkeys.h
		trace.h)

SET(SASL_LIBRARY)
IF(ENABLE_MEMCACHED_SASL)
  SET(SASL_LIBRARY "sasl2")
ENDIF()

MYSQL_ADD_PLUGIN(libmemcached ${MEMCACHED_SOURCES}
                 LINK_LIBRARIES ${LIBEVENT_LIBRARIES}
                 memcached_utilities ${SASL_LIBRARY}
		 MODULE_ONLY MODULE_OUTPUT_NAME "libmemcached")
