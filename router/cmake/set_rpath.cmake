# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(RPATH_ORIGIN "@loader_path")
elseif(CMAKE_SYSTEM_NAME STREQUAL "CYGWIN")
  set(RPATH_ORIGIN "\$ORIGIN")
else()
  set(RPATH_ORIGIN "\$ORIGIN")
endif()

# relative to CMAKE_INSTALL_PREFIX or absolute
SET(ROUTER_INSTALL_BINDIR "${INSTALL_BINDIR}")

# If router libdir not set, use MySQL libdir (for libharness and libmysqlrouter)
IF("${ROUTER_INSTALL_LIBDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_LIBDIR "${INSTALL_LIBDIR}")
ENDIF()

# If router plugindir not set, use $router_install_libdir/mysqlrouter
IF("${ROUTER_INSTALL_PLUGINDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_PLUGINDIR "${ROUTER_INSTALL_LIBDIR}/mysqlrouter")
ENDIF()

IF("${ROUTER_INSTALL_DOCDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_DOCDIR "${INSTALL_DOCDIR}")
ENDIF()
IF("${ROUTER_INSTALL_SHAREDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_SHAREDIR "${INSTALL_SHAREDIR}")
ENDIF()

# if are _pure_ STANDALONE we can write into data/ as it is all ours
# if we are shared STANDALONE with the the server, we shouldn't write
# into the server's data/ as that would create a "schemadir" in
# mysql-servers sense
IF(INSTALL_LAYOUT STREQUAL "WIN")
  SET(ROUTER_INSTALL_CONFIGDIR ".")
  SET(ROUTER_INSTALL_DATADIR ".")
  SET(ROUTER_INSTALL_LOGDIR "log/mysqlrouter")
  SET(ROUTER_INSTALL_RUNTIMEDIR ".")
ELSEIF(INSTALL_LAYOUT STREQUAL "STANDALONE")
  SET(ROUTER_INSTALL_CONFIGDIR ".")
  SET(ROUTER_INSTALL_DATADIR "data")
  SET(ROUTER_INSTALL_LOGDIR ".")
  SET(ROUTER_INSTALL_RUNTIMEDIR "run")
ELSEIF(INSTALL_LAYOUT STREQUAL "DEFAULT")
  SET(_destdir "/var/local/mysqlrouter")
  SET(ROUTER_INSTALL_CONFIGDIR "etc/mysqlrouter")
  SET(ROUTER_INSTALL_DATADIR "${_destdir}/data")
  SET(ROUTER_INSTALL_LOGDIR "${_destdir}/log")
  SET(ROUTER_INSTALL_RUNTIMEDIR "${_destdir}/run")
ELSEIF(INSTALL_LAYOUT STREQUAL "SVR4")
  SET(ROUTER_INSTALL_CONFIGDIR "/etc/opt/mysqlrouter")
  SET(ROUTER_INSTALL_DATADIR "/var/opt/mysqlrouter")
  SET(ROUTER_INSTALL_LOGDIR "/var/opt/mysqlrouter")
  SET(ROUTER_INSTALL_RUNTIMEDIR "/var/opt/mysqlrouter")
ELSE()
  SET(ROUTER_INSTALL_CONFIGDIR "/etc/mysqlrouter")
  SET(ROUTER_INSTALL_DATADIR "/var/lib/mysqlrouter")
  SET(ROUTER_INSTALL_LOGDIR "/var/log/mysqlrouter")
  SET(ROUTER_INSTALL_RUNTIMEDIR "/var/run/mysqlrouter")
ENDIF()

SET(CMAKE_INSTALL_RPATH)
IF(INSTALL_LAYOUT STREQUAL "STANDALONE" OR INSTALL_LAYOUT STREQUAL "DEFAULT" OR
   INSTALL_LAYOUT STREQUAL "WIN")
  # rpath for lib/mysqlrouter/ plugins that want to find lib/
  SET(RPATH_PLUGIN_TO_LIB "${RPATH_ORIGIN}/../")
  SET(RPATH_PLUGIN_TO_PLUGIN "${RPATH_ORIGIN}/")
  # rpath for lib/ libraries that want to find other libs in lib/
  SET(RPATH_LIBRARY_TO_LIB "${RPATH_ORIGIN}/")
  # rpath for bin/ binaries that want to find other libs in lib/
  SET(RPATH_BINARY_TO_LIB "${RPATH_ORIGIN}/../${ROUTER_INSTALL_LIBDIR}/")

ELSE()
  IF(INSTALL_LAYOUT STREQUAL "SVR4")
    SET(_dest_dir "/opt/mysql/mysql-router/")
  ELSE()
    SET(_dest_dir "${CMAKE_INSTALL_PREFIX}")
  ENDIF()
  # rpath for lib/mysqlrouter/ plugins that want to find lib/
  SET(RPATH_PLUGIN_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR}")
  SET(RPATH_PLUGIN_TO_PLUGIN "${_dest_dir}/${ROUTER_INSTALL_PLUGINDIR}")
  # rpath for lib/ libraries that want to find other libs in lib/
  SET(RPATH_LIBRARY_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR}")
  # rpath for bin/ binaries that want to find other libs in lib/
  SET(RPATH_BINARY_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR}")

ENDIF()

# plugins may depend on other plugins
# plugins may depend on libs in lib/
# executables may depend on libs in lib/
SET(ROUTER_INSTALL_RPATH
  ${RPATH_PLUGIN_TO_LIB}
  ${RPATH_PLUGIN_TO_PLUGIN}
  ${RPATH_LIBRARY_TO_LIB}
  ${RPATH_BINARY_TO_LIB})
LIST(APPEND CMAKE_INSTALL_RPATH
  ${RPATH_PLUGIN_TO_LIB}
  ${RPATH_PLUGIN_TO_PLUGIN}
  ${RPATH_LIBRARY_TO_LIB}
  ${RPATH_BINARY_TO_LIB})


LIST(REMOVE_DUPLICATES CMAKE_INSTALL_RPATH)

SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#MESSAGE(STATUS "Router install directories:")
#MESSAGE(STATUS "- bindir: ${ROUTER_INSTALL_BINDIR}")
#MESSAGE(STATUS "- configdir: ${ROUTER_INSTALL_CONFIGDIR}")
#MESSAGE(STATUS "- docdir: ${ROUTER_INSTALL_DOCDIR}")
#MESSAGE(STATUS "- libdir: ${ROUTER_INSTALL_LIBDIR}")
#MESSAGE(STATUS "- plugindir: ${ROUTER_INSTALL_PLUGINDIR}")
#MESSAGE(STATUS "- datadir: ${ROUTER_INSTALL_DATADIR}")
#MESSAGE(STATUS "- sharedir: ${ROUTER_INSTALL_SHAREDIR}")
#MESSAGE(STATUS "- rpath: ${CMAKE_INSTALL_RPATH}")
