# Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/memcached
                    ${CMAKE_CURRENT_SOURCE_DIR}/utilities 
                    ${CMAKE_CURRENT_SOURCE_DIR}/daemon)

INCLUDE_DIRECTORIES(SYSTEM ${LIBEVENT_INCLUDE_DIRS})

IF(${CMAKE_COMPILER_IS_GNUCC})
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
ELSEIF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xc99=all")
ENDIF()


# This is third-party code so we reduce the compiler warnings.
# These are numerous, so memcached cannot currently be built with -Werror.
IF(CMAKE_C_FLAGS)
  STRING(REPLACE "-Wextra "  " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  STRING(REPLACE "-Wdeclaration-after-statement" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  STRING(REPLACE "-Wvla"    "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  STRING(REPLACE "-Werror"  "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
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

ADD_CONVENIENCE_LIBRARY(ndb_memcached_utilities ${LIBMEMCACHED_UTILITIES_SOURCES})
IF(HAVE_LIBDL)
target_link_libraries(ndb_memcached_utilities dl)
ENDIF()

SET(MEMCACHED_SOURCES
                daemon/cache.h
                daemon/cache.c
                config_static.h
                daemon/daemon.c
                daemon/hash.c
                daemon/hash.h
                daemon/memcached.c
                daemon/memcached.h
                daemon/sasl_defs.h
                daemon/stats.c
                daemon/stats.h
                daemon/thread.c
                daemon/topkeys.c
                daemon/topkeys.h
                trace.h)

add_executable(memcached ${MEMCACHED_SOURCES})

target_link_libraries(memcached 
                      ndb_memcached_utilities
                      ${LIBEVENT_LIBRARIES}
                      ${LIBM} ${LIBNSL} ${LIBSOCKET})
IF(HAVE_LIBDL)
target_link_libraries(memcached dl)
ENDIF()

if(LIBRT)
  target_link_libraries(memcached rt)
endif()

CHECK_INCLUDE_FILE("link.h"          HAVE_LINK_H)
CHECK_INCLUDE_FILE("stdbool.h"       HAVE_STDBOOL_H)
CHECK_INCLUDE_FILE("sysexits.h"      HAVE_SYSEXITS_H)
CHECK_INCLUDE_FILE("netdb.h"         HAVE_NETDB_H)
CHECK_INCLUDE_FILE("sys/uio.h"       HAVE_SYS_UIO_H)
CHECK_INCLUDE_FILE("signal.h"        HAVE_SIGIGNORE)
CHECK_INCLUDE_FILES("sys/types.h;netinet/tcp.h" HAVE_NETINET_TCP_H)
CHECK_FUNCTION_EXISTS(htonll         HAVE_HTONLL)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config_tests.in 
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(TARGETS memcached DESTINATION ${INSTALL_SBINDIR}) 


