# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(APPLE)
  SET(RPATH_ORIGIN "@loader_path")
ELSE()
  SET(RPATH_ORIGIN "\$ORIGIN")
ENDIF()

SET(CMAKE_INSTALL_RPATH)
IF(INSTALL_LAYOUT STREQUAL "STANDALONE"
    OR INSTALL_LAYOUT STREQUAL "SVR4")
  # rpath for lib/mysqlrouter/ plugins that want to find lib/
  IF(LINUX)
    SET(RPATH_PLUGIN_TO_LIB "${RPATH_ORIGIN}/private")
  ELSE()
    SET(RPATH_PLUGIN_TO_LIB "${RPATH_ORIGIN}/../")
  ENDIF()
  SET(RPATH_PLUGIN_TO_PLUGIN "${RPATH_ORIGIN}/")
  # rpath for lib/ libraries that want to find other libs in lib/
  SET(RPATH_LIBRARY_TO_LIB "${RPATH_ORIGIN}/")
  # rpath for bin/ binaries that want to find other libs in lib/
  SET(RPATH_BINARY_TO_LIB "${RPATH_ORIGIN}/../${ROUTER_INSTALL_LIBDIR}/")

ELSE()
  SET(_dest_dir "${CMAKE_INSTALL_PREFIX}")
  # rpath for lib/mysqlrouter/ plugins that want to find lib/
  IF(LINUX)
    SET(PLUGIN_TO_LIB_ORIG "${RPATH_ORIGIN}/private")
  ELSE()
    SET(PLUGIN_TO_LIB_ORIG "${RPATH_ORIGIN}/../")
  ENDIF()
  SET(RPATH_PLUGIN_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR}/;"
                          "${PLUGIN_TO_LIB_ORIG}")
  SET(RPATH_PLUGIN_TO_PLUGIN "${_dest_dir}/${ROUTER_INSTALL_PLUGINDIR}/;"
                             "${RPATH_ORIGIN}/")
  # rpath for lib/ libraries that want to find other libs in lib/
  SET(RPATH_LIBRARY_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR};"
                           "${RPATH_ORIGIN}/")
  # rpath for bin/ binaries that want to find other libs in lib/
  SET(RPATH_BINARY_TO_LIB "${_dest_dir}/${ROUTER_INSTALL_LIBDIR};"
                          "${RPATH_ORIGIN}/../${ROUTER_INSTALL_LIBDIR}/")

ENDIF()

# plugins may depend on other plugins
# plugins may depend on libs in lib/
# executables may depend on libs in lib/
SET(ROUTER_INSTALL_RPATH
  ${RPATH_PLUGIN_TO_LIB}
  ${RPATH_PLUGIN_TO_PLUGIN}
  ${RPATH_LIBRARY_TO_LIB}
  ${RPATH_BINARY_TO_LIB}
  )
LIST(APPEND CMAKE_INSTALL_RPATH
  ${RPATH_PLUGIN_TO_LIB}
  ${RPATH_PLUGIN_TO_PLUGIN}
  ${RPATH_LIBRARY_TO_LIB}
  ${RPATH_BINARY_TO_LIB}
  )

IF(LINUX_INSTALL_RPATH_ORIGIN)
  LIST(APPEND ROUTER_INSTALL_RPATH "\$ORIGIN/../private")
  LIST(APPEND CMAKE_INSTALL_RPATH "\$ORIGIN/../private")
ENDIF()

LIST(REMOVE_DUPLICATES CMAKE_INSTALL_RPATH)

SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#MESSAGE(STATUS "Router install directories:")
#MESSAGE(STATUS "ROUTER_INSTALL_RPATH ${ROUTER_INSTALL_RPATH}")
#MESSAGE(STATUS "- bindir: ${ROUTER_INSTALL_BINDIR}")
#MESSAGE(STATUS "- configdir: ${ROUTER_INSTALL_CONFIGDIR}")
#MESSAGE(STATUS "- docdir: ${ROUTER_INSTALL_DOCDIR}")
#MESSAGE(STATUS "- libdir: ${ROUTER_INSTALL_LIBDIR}")
#MESSAGE(STATUS "- plugindir: ${ROUTER_INSTALL_PLUGINDIR}")
#MESSAGE(STATUS "- datadir: ${ROUTER_INSTALL_DATADIR}")
#MESSAGE(STATUS "- rpath: ${CMAKE_INSTALL_RPATH}")
