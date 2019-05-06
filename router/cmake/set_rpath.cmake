# Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

# This follows pattern from cmake/install_layout.cmake
#
# Supported layouts here are STANDALONE, WIN, RPM, DEB, SVR4 or
# FREEBSD.
# Layouts GLIBC, OSX, TARGZ and SLES seems unused and are similar to
# STANDALONE or RPM any way.

# Variables ROUTER_INSTALL_${X}DIR, where
#  X = BIN, LIB and DOC is using
# inheritance from correspondig server variable.
# While, when
#  X = CONFIG, DATA, LOG and RUNTIME
# default value is set by install layouts below.
# finally, when
#  X = plugin
# ROUTER_INSTALL_LIBDIR/mysqlrouter is used by default.

# Relative to CMAKE_INSTALL_PREFIX or absolute
IF("${ROUTER_INSTALL_BINDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_BINDIR "${INSTALL_BINDIR}")
ENDIF()

# If router libdir not set, use MySQL libdir (for libharness and libmysqlrouter)
IF("${ROUTER_INSTALL_LIBDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_LIBDIR "${INSTALL_LIBDIR}")
ENDIF()

# If router plugindir not set, use $router_install_libdir[/mysqlrouter]
IF("${ROUTER_INSTALL_PLUGINDIR}" STREQUAL "")
  IF(WIN32)
    SET(ROUTER_INSTALL_PLUGINDIR "${ROUTER_INSTALL_LIBDIR}")
  ELSE()
    SET(ROUTER_INSTALL_PLUGINDIR "${ROUTER_INSTALL_LIBDIR}/mysqlrouter")
  ENDIF()
ENDIF()

IF("${ROUTER_INSTALL_DOCDIR}" STREQUAL "")
  SET(ROUTER_INSTALL_DOCDIR "${INSTALL_DOCDIR}")
ENDIF()

IF(NOT ROUTER_INSTALL_LAYOUT)
  SET(DEFAULT_ROUTER_INSTALL_LAYOUT "${INSTALL_LAYOUT}")
ENDIF()

SET(ROUTER_INSTALL_LAYOUT "${DEFAULT_ROUTER_INSTALL_LAYOUT}"
  CACHE
  STRING
  "Installation directory layout. Options are:  WIN (as in zip installer), STANDALONE,  RPM, DEB or FREEBSD")

# If are _pure_ STANDALONE we can write into data/ as it is all ours
# if we are shared STANDALONE with the the server, we shouldn't write
# into the server's data/ as that would create a "schemadir" in
# mysql-servers sense
#
# STANDALONE layout
#
SET(ROUTER_INSTALL_CONFIGDIR_STANDALONE  ".")
SET(ROUTER_INSTALL_DATADIR_STANDALONE    "var/lib/mysqlrouter")
SET(ROUTER_INSTALL_LOGDIR_STANDALONE     ".")
SET(ROUTER_INSTALL_RUNTIMEDIR_STANDALONE "run")
#
# Win layout
#
SET(ROUTER_INSTALL_CONFIGDIR_WIN  ".")
SET(ROUTER_INSTALL_DATADIR_WIN    ".")
SET(ROUTER_INSTALL_LOGDIR_WIN     "log/mysqlrouter")
SET(ROUTER_INSTALL_RUNTIMEDIR_WIN ".")
#
# FreeBSD layout
#
SET(ROUTER_INSTALL_CONFIGDIR_FREEBSD  "/usr/local/etc/mysqlrouter")
SET(ROUTER_INSTALL_DATADIR_FREEBSD    "/var/db/mysqlrouter")
SET(ROUTER_INSTALL_LOGDIR_FREEBSD     "/var/log/mysqlrouter")
SET(ROUTER_INSTALL_RUNTIMEDIR_FREEBSD "/var/run/mysqlrouter")
#
# RPM layout
#
SET(ROUTER_INSTALL_CONFIGDIR_RPM    "/etc/mysqlrouter")
SET(ROUTER_INSTALL_DATADIR_RPM      "/var/lib/mysqlrouter")
SET(ROUTER_INSTALL_LOGDIR_RPM       "/var/log/mysqlrouter")
IF (LINUX_FEDORA)
  SET(ROUTER_INSTALL_RUNTIMEDIR_RPM "/run/mysqlrouter")
ELSE()
  SET(ROUTER_INSTALL_RUNTIMEDIR_RPM "/var/run/mysqlrouter")
ENDIF()
#
# DEB layout
#
SET(ROUTER_INSTALL_CONFIGDIR_DEB  "/etc/mysqlrouter")
SET(ROUTER_INSTALL_DATADIR_DEB    "/var/run/mysqlrouter")
SET(ROUTER_INSTALL_LOGDIR_DEB     "/var/log/mysqlrouter")
SET(ROUTER_INSTALL_RUNTIMEDIR_DEB "/var/run/mysqlrouter")

# Mimic cmake/install_layout.cmake:
# Set ROUTER_INSTALL_FOODIR variables for chosen layout for example,
# ROUTER_INSTALL_CONFIGDIR will be defined as
# ${ROUTER_INSTALL_CONFIGDIR_STANDALONE} by default if STANDALONE
# layout is chosen.
FOREACH(directory
    CONFIG
    DATA
    LOG
    RUNTIME
    )
  SET(ROUTER_INSTALL_${directory}DIR
    ${ROUTER_INSTALL_${directory}DIR_${ROUTER_INSTALL_LAYOUT}}
    CACHE STRING "Router ${directory} installation directory")
  MARK_AS_ADVANCED(ROUTER_INSTALL_${directory}DIR)
ENDFOREACH()

IF(APPLE)
  SET(RPATH_ORIGIN "@loader_path")
ELSE()
  SET(RPATH_ORIGIN "\$ORIGIN")
ENDIF()

SET(CMAKE_INSTALL_RPATH)
IF(INSTALL_LAYOUT STREQUAL "STANDALONE" OR INSTALL_LAYOUT STREQUAL "WIN"
    OR INSTALL_LAYOUT STREQUAL "SVR4")
  # rpath for lib/mysqlrouter/ plugins that want to find lib/
  SET(RPATH_PLUGIN_TO_LIB "${RPATH_ORIGIN}/../")
  SET(RPATH_PLUGIN_TO_PLUGIN "${RPATH_ORIGIN}/")
  # rpath for lib/ libraries that want to find other libs in lib/
  SET(RPATH_LIBRARY_TO_LIB "${RPATH_ORIGIN}/")
  # rpath for bin/ binaries that want to find other libs in lib/
  SET(RPATH_BINARY_TO_LIB "${RPATH_ORIGIN}/../${ROUTER_INSTALL_LIBDIR}/")

ELSE()
  SET(_dest_dir "${CMAKE_INSTALL_PREFIX}")
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
#MESSAGE(STATUS "- rpath: ${CMAKE_INSTALL_RPATH}")
