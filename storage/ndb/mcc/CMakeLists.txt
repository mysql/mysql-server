# Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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

IF(WITHOUT_SERVER)
  OPTION(WITH_MCC "Include MySQL Cluster Configurator" OFF)
ELSE()
  OPTION(WITH_MCC "Include MySQL Cluster Configurator" ON)
ENDIF()

if(NOT WITH_MCC)
  MESSAGE(STATUS "Skipping MySQL Cluster Configurator")
  RETURN()
ENDIF()
MESSAGE(STATUS "Building MySQL Cluster Configurator")

INCLUDE(ndb_require_variable)

SET(PB2DRIVE $ENV{PB2DRIVE})
IF(WIN32 AND PB2DRIVE)
  # Make sure that config variable pointing out the location of
  # python is properly provided by release build system
  # when building on Windows
  NDB_REQUIRE_VARIABLE(MCC_PYTHON_TO_BUNDLE)
ENDIF()

SET(MCC_SCRIPT_BASE "ndb_setup")
SET(MCC_SCRIPT_NAME "${MCC_SCRIPT_BASE}.py")

SET(MCC_LAUNCH_BASE "setup")
SET(MCC_BROWSER_START_PAGE "welcome.html")

SET(MCC_INSTALL_BINDIR "${INSTALL_BINDIR}")
SET(MCC_INSTALL_SUBDIR "${INSTALL_MYSQLSHAREDIR}/mcc")
SET(MCC_INSTALL_FRONTENDDIR "${MCC_INSTALL_SUBDIR}/frontend")

ADD_SUBDIRECTORY(frontend)

IF(WIN32 AND MCC_PYTHON_TO_BUNDLE)
  FILE(TO_CMAKE_PATH "${MCC_PYTHON_TO_BUNDLE}" MCC_PYTHON_TO_BUNDLE_CMP)
  SET(MCC_PYTHON_TO_BUNDLE "${MCC_PYTHON_TO_BUNDLE_CMP}")
  MESSAGE(STATUS "Checking ${MCC_PYTHON_TO_BUNDLE}...")
  FIND_PROGRAM(BUNDLEPY_EXE NAMES python python.exe
    PATHS ${MCC_PYTHON_TO_BUNDLE} NO_DEFAULT_PATH)

  IF(BUNDLEPY_EXE)
    EXECUTE_PROCESS(COMMAND ${BUNDLEPY_EXE} "-V" RESULT_VARIABLE BUNDLEPY_RES
      ERROR_VARIABLE BUNDLEPY_ERR)

    IF(NOT BUNDLEPY_RES)
      STRING(REPLACE "Python " "" BUNDLEPY_VER ${BUNDLEPY_ERR})

      IF(NOT BUNDLEPY_VER VERSION_LESS "2.6"
          AND BUNDLEPY_VER VERSION_LESS "3.0")
        SET(ENV{PYTHONLIB} "${MCC_PYTHON_TO_BUNDLE}/Lib")
        EXECUTE_PROCESS(COMMAND "${BUNDLEPY_EXE}" "-c" "import paramiko"
          RESULT_VARIABLE MCC_PYBUNDLE_IMPORT_PARAMIKO_RES ERROR_QUIET)
        IF(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)
          MESSAGE("${MCC_PYTHON_TO_BUNDLE} does not appear to include Paramiko")
        ELSE(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)
          MESSAGE(STATUS "${MCC_PYTHON_TO_BUNDLE} looks OK")
        ENDIF(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)
      ELSE(NOT BUNDLEPY_VER VERSION_LESS "2.6" AND
          BUNDLEPY_VER VERSION_LESS "3.0")
        MESSAGE("Unsupported python version: ${BUNDLEPY_VER}")
      ENDIF(NOT BUNDLEPY_VER VERSION_LESS "2.6" AND
        BUNDLEPY_VER VERSION_LESS "3.0")
    ELSE(NOT BUNDLEPY_RES)
      MESSAGE("${BUNDLEPY_EXE} does not appear to be a valid executable")
    ENDIF(NOT BUNDLEPY_RES)

  ELSE(BUNDLEPY_EXE)
    MESSAGE("Failed to locate a python executable in ${MCC_PYTHON_TO_BUNDLE}")
  ENDIF(BUNDLEPY_EXE)

  INSTALL(DIRECTORY "${MCC_PYTHON_TO_BUNDLE}/"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python" COMPONENT ClusterTools)

  FILE(TO_NATIVE_PATH "${MCC_INSTALL_SUBDIR}/Python" MCC_PY_INSTALL_DIR)
  # Create setup.bat and setup-debug.bat with suitable content
  CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/setup.bat.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}.bat")

  CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/setup-debug.bat.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}-debug.bat")

  INSTALL(PROGRAMS
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}.bat"
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}-debug.bat"
    DESTINATION "."
    COMPONENT ClusterTools)

ENDIF()

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/mcc_config.py.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mcc_config_for_install.py")
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcc_config_for_install.py"
  DESTINATION "${MCC_INSTALL_BINDIR}"
  COMPONENT ClusterTools
  RENAME "mcc_config.py")

INSTALL(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_NAME}"
  DESTINATION "${MCC_INSTALL_BINDIR}"
  COMPONENT ClusterTools)

INSTALL(FILES clusterhost.py remote_clusterhost.py request_handler.py
  util.py config_parser.py cfg.pem DESTINATION "${MCC_INSTALL_SUBDIR}"
  COMPONENT ClusterTools)

INSTALL(DIRECTORY tst DESTINATION "${MCC_INSTALL_SUBDIR}"
  COMPONENT ClusterTools)


# Additional utility targets for MCC development
OPTION(WITH_MCC_DEV "Include MySQL Cluster Configurator dev targets" OFF)
MARK_AS_ADVANCED(WITH_MCC_DEV)
IF (WITH_MCC_DEV)
  MESSAGE(STATUS "Adding MCC dev targets")

  IF(WIN32 AND NOT CMAKE_INSTALL_CONFIG_NAME)
    SET(CMAKE_INSTALL_CONFIG_NAME $(Configuration))
  ENDIF()

  # Convenience target for mcc development which installs only the mcc files,
  # this is much faster than installing everything
  ADD_CUSTOM_TARGET(mcc_install
    ${CMAKE_COMMAND}
    "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_INSTALL_CONFIG_NAME}"
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
    -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake"
    VERBATIM)

  # Convenience target for mcc development which runs the mcc tests
  # on installed files
  ADD_CUSTOM_TARGET(mcc_run
    COMMAND ${CMAKE_COMMAND} -E remove mcc_log.txt
    COMMAND python
    "${CMAKE_INSTALL_PREFIX}/${MCC_INSTALL_BINDIR}/${MCC_SCRIPT_NAME}"
    -N "" -d DEBUG -o mcc_log.txt
    VERBATIM)
  ADD_DEPENDENCIES(mcc_run mcc_install)

  ADD_CUSTOM_TARGET(mcc_run_in_source
    python "${MCC_SCRIPT_BASE}_in_source.py" -d DEBUG -o mcc_in_source_log.txt
    VERBATIM)
  ADD_CUSTOM_TARGET(mcc_punit
    python "${CMAKE_CURRENT_SOURCE_DIR}/tst/tst_main.py"
    VERBATIM)
ENDIF()
