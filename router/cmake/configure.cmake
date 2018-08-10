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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


# installed executable location (used in config.h)
IF(IS_ABSOLUTE "${ROUTER_INSTALL_BINDIR}")
  SET(ROUTER_BINDIR ${ROUTER_INSTALL_BINDIR})
ELSE()
  SET(ROUTER_BINDIR ${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_BINDIR})
ENDIF()

# Configuration folder (config_folder configuration option)
IF(WIN32)
  SET(_configdir "ENV{APPDATA}")
ELSE()
  IF(IS_ABSOLUTE ${ROUTER_INSTALL_CONFIGDIR})
    SET(_configdir "${ROUTER_INSTALL_CONFIGDIR}")
  ELSEIF(INSTALL_CONFIGDIR STREQUAL ".")
    # Current working directory
    SET(_configdir "${ROUTER_INSTALL_CONFIGDIR}")
  ELSE()
    SET(_configdir "${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_CONFIGDIR}")
  ENDIF()
ENDIF()
SET(ROUTER_CONFIGDIR ${_configdir})
UNSET(_configdir)

# Logging folder (logging_folder configuration option)
IF(WIN32)
  SET(_logdir "ENV{APPDATA}\\\\log")
ELSE()
  # logging folder can be set to empty to log to console
  IF(IS_ABSOLUTE "${ROUTER_INSTALL_LOGDIR}")
    SET(_logdir ${ROUTER_INSTALL_LOGDIR})
  ELSEIF(NOT ROUTER_INSTALL_LOGDIR)
    SET(_logdir "/var/log/mysqlrouter/")
  ELSE()
    SET(_logdir ${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_LOGDIR})
  ENDIF()
ENDIF()
SET(ROUTER_LOGDIR ${_logdir}
  CACHE STRING "Location of log files; empty is console (logging_folder)")
UNSET(_logdir)

# Runtime folder (runtime_folder configuration option)
IF(WIN32)
  SET(_runtimedir "ENV{APPDATA}")
ELSE()
  IF(IS_ABSOLUTE "${ROUTER_INSTALL_RUNTIMEDIR}")
    SET(_runtimedir ${ROUTER_INSTALL_RUNTIMEDIR})
  ELSEIF(NOT ROUTER_INSTALL_RUNTIMEDIR)
    SET(_logdir "/var/run/mysqlrouter/")
  ELSE()
    SET(_runtimedir ${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_RUNTIMEDIR})
  ENDIF()
ENDIF()
SET(ROUTER_RUNTIMEDIR ${_runtimedir}
  CACHE STRING "Location runtime files such as PID file (runtime_folder)")
UNSET(_runtimedir)

# Plugin folder (plugin_folder configuration option)
IF(IS_ABSOLUTE "${ROUTER_INSTALL_PLUGINDIR}")
  SET(_plugindir ${ROUTER_INSTALL_PLUGINDIR})
ELSE()
  SET(_plugindir ${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_PLUGINDIR})
ENDIF()
SET(ROUTER_PLUGINDIR ${_plugindir}
  CACHE STRING "Location MySQL Router plugins (plugin_folder)")
UNSET(_plugindir)

# Data folder (data_folder configuration option)
IF(WIN32)
  SET(_datadir "ENV{APPDATA}\\\\data")
ELSE()
  IF(IS_ABSOLUTE "${ROUTER_INSTALL_DATADIR}")
    SET(_datadir ${ROUTER_INSTALL_DATADIR})
  ELSEIF(NOT ROUTER_INSTALL_DATADIR)
    SET(_datadir "/var/lib/mysqlrouter/")
  ELSE()
    SET(_datadir "${CMAKE_INSTALL_PREFIX}/${ROUTER_INSTALL_DATADIR}")
  ENDIF()
ENDIF()
SET(ROUTER_DATADIR ${_datadir}
  CACHE STRING "Location of data files such as keyring file")
UNSET(_datadir)

IF(INSTALL_LAYOUT STREQUAL "STANDALONE")
  SET(ROUTER_PLUGINDIR "{origin}/../${ROUTER_INSTALL_PLUGINDIR}")
  SET(ROUTER_CONFIGDIR "{origin}/../${ROUTER_INSTALL_CONFIGDIR}")
  SET(ROUTER_RUNTIMEDIR "{origin}/../${ROUTER_INSTALL_RUNTIMEDIR}")
  SET(ROUTER_LOGDIR "{origin}/../${ROUTER_INSTALL_LOGDIR}")
  SET(ROUTER_DATADIR "{origin}/../${ROUTER_INSTALL_DATADIR}")
ENDIF()

# Default configuration file locations (similar to MySQL Server)
IF(WIN32)
  SET(CONFIG_FILE_LOCATIONS
      "${ROUTER_CONFIGDIR}/${MYSQL_ROUTER_INI}"
      "ENV{APPDATA}/${MYSQL_ROUTER_INI}"
      )
ELSE()
  SET(CONFIG_FILE_LOCATIONS
      "${ROUTER_CONFIGDIR}/${MYSQL_ROUTER_INI}"
      "ENV{HOME}/.${MYSQL_ROUTER_INI}"
      )
ENDIF()
SET(CONFIG_FILES ${CONFIG_FILE_LOCATIONS})

# check if platform supports prlimit()
INCLUDE(CMakePushCheckState)
cmake_push_check_state()
cmake_reset_check_state()
INCLUDE(CheckSymbolExists)
SET(CMAKE_REQUIRED_FLAGS -D_GNU_SOURCE)
check_symbol_exists(prlimit sys/resource.h HAVE_PRLIMIT)
cmake_pop_check_state()

CONFIGURE_FILE(config.h.in router_config.h @ONLY)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
