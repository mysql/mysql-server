# Copyright (c) 2011, 2020, Oracle and/or its affiliates.
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
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET(MEMCACHED_HOME "" CACHE PATH "Path to installed Memcached 1.6")


if(WITH_BUNDLED_MEMCACHED) 
  # Use bundled memcached
  set(MEMCACHED_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extra/memcached)
  set(MEMCACHED_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extra/memcached/include)
  set(MEMCACHED_UTILITIES_LIBRARY ndb_memcached_utilities)
  set(MEMCACHED_BIN_PATH ${CMAKE_INSTALL_PREFIX}/${INSTALL_SBINDIR}/memcached)
else()   

  # Find an installed memcached
  find_path(MEMCACHED_ROOT_DIR bin/engine_testapp
    HINTS 
      $ENV{MEMCACHED_HOME}
      ${MEMCACHED_HOME}
      ${CMAKE_INSTALL_PREFIX}
    PATHS
      /usr/local  /usr
      /opt/local  /opt
      ~/Library/Frameworks  /Library/Frameworks
  )  

  find_path(MEMCACHED_INCLUDE_DIR memcached/engine_testapp.h
    HINTS  ${MEMCACHED_ROOT_DIR}
    PATH_SUFFIXES include
  )

  find_library(MEMCACHED_UTILITIES_LIBRARY
    NAMES memcached_utilities
    HINTS  ${MEMCACHED_ROOT_DIR}
    PATH_SUFFIXES lib/memcached lib memcached/lib
  )
  
  set(MEMCACHED_BIN_PATH ${MEMCACHED_ROOT_DIR}/bin/memcached)
endif()

if(MEMCACHED_ROOT_DIR AND MEMCACHED_INCLUDE_DIR AND MEMCACHED_UTILITIES_LIBRARY) 
  set(MEMCACHED_FOUND TRUE)
else()
  set(MEMCACHED_FOUND FALSE)
endif()

mark_as_advanced(MEMCACHED_ROOT_DIR 
                 MEMCACHED_INCLUDE_DIR 
                 MEMCACHED_UTILITIES_LIBRARY
                 MEMCACHED_BIN_PATH)
