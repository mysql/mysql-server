# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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
  option(WITH_MCC "Include MySQL Cluster Configurator" OFF)
ELSE()
  option(WITH_MCC "Include MySQL Cluster Configurator" ON)
ENDIF()

if(NOT WITH_MCC)
  message(STATUS "Skipping MySQL Cluster Configurator")
  return()
endif()
MESSAGE(STATUS "Building MySQL Cluster Configurator")

INCLUDE(ndb_require_variable)

set(PB2DRIVE $ENV{PB2DRIVE})
if(WIN32 AND PB2DRIVE)
  # Make sure that config variable pointing out the location of
  # python is properly provided by release build system
  # when building on Windows
  ndb_require_variable(MCC_PYTHON_TO_BUNDLE)
endif() 

set(MCC_SCRIPT_BASE "ndb_setup")
set(MCC_SCRIPT_NAME "${MCC_SCRIPT_BASE}.py")

set(MCC_LAUNCH_BASE "setup")
set(MCC_BROWSER_START_PAGE "welcome.html")

set(MCC_INSTALL_BINDIR "${INSTALL_BINDIR}")
set(MCC_INSTALL_SUBDIR "${INSTALL_MYSQLSHAREDIR}/mcc")
set(MCC_INSTALL_FRONTENDDIR "${MCC_INSTALL_SUBDIR}/frontend")

ADD_SUBDIRECTORY(host_info)
ADD_SUBDIRECTORY(frontend)

if(WIN32 AND MCC_PYTHON_TO_BUNDLE)
  file(TO_CMAKE_PATH "${MCC_PYTHON_TO_BUNDLE}" MCC_PYTHON_TO_BUNDLE_CMP)
  set(MCC_PYTHON_TO_BUNDLE "${MCC_PYTHON_TO_BUNDLE_CMP}")
  message(STATUS "Checking ${MCC_PYTHON_TO_BUNDLE}...")
  find_program(BUNDLEPY_EXE NAMES python python.exe 
    PATHS ${MCC_PYTHON_TO_BUNDLE} NO_DEFAULT_PATH)

  if(BUNDLEPY_EXE)
    execute_process(COMMAND ${BUNDLEPY_EXE} "-V" RESULT_VARIABLE BUNDLEPY_RES 
      ERROR_VARIABLE BUNDLEPY_ERR)

    if(NOT BUNDLEPY_RES)
      string(REPLACE "Python " "" BUNDLEPY_VER ${BUNDLEPY_ERR})
      
      if(NOT BUNDLEPY_VER VERSION_LESS "2.6" 
          AND BUNDLEPY_VER VERSION_LESS "3.0")
        SET(ENV{PYTHONLIB} "${MCC_PYTHON_TO_BUNDLE}/Lib")
        execute_process(COMMAND "${BUNDLEPY_EXE}" "-c" "import paramiko" 
          RESULT_VARIABLE MCC_PYBUNDLE_IMPORT_PARAMIKO_RES ERROR_QUIET)
        if(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)
          message("${MCC_PYTHON_TO_BUNDLE} does not appear to include Paramiko")
        else(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)
          message(STATUS "${MCC_PYTHON_TO_BUNDLE} looks OK")
        endif(MCC_PYBUNDLE_IMPORT_PARAMIKO_RES)        
      else(NOT BUNDLEPY_VER VERSION_LESS "2.6" AND 
          BUNDLEPY_VER VERSION_LESS "3.0")
        message("Unsupported python version: ${BUNDLEPY_VER}")
      endif(NOT BUNDLEPY_VER VERSION_LESS "2.6" AND 
        BUNDLEPY_VER VERSION_LESS "3.0")
    else(NOT BUNDLEPY_RES)
      message("${BUNDLEPY_EXE} does not appear to be a valid executable")
    endif(NOT BUNDLEPY_RES)

  else(BUNDLEPY_EXE)
    message("Failed to locate a python executable in ${MCC_PYTHON_TO_BUNDLE}")
  endif(BUNDLEPY_EXE)

  install(DIRECTORY "${MCC_PYTHON_TO_BUNDLE}/" 
	DESTINATION "${MCC_INSTALL_SUBDIR}/Python" COMPONENT ClusterTools)
  
  FILE(TO_NATIVE_PATH "${MCC_INSTALL_SUBDIR}/Python" MCC_PY_INSTALL_DIR)
  # Create setup.bat and setup-debug.bat with suitable content
  CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/setup.bat.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}.bat")

  CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/setup-debug.bat.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}-debug.bat")

  install(PROGRAMS 
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}.bat"  
    "${CMAKE_CURRENT_BINARY_DIR}/${MCC_LAUNCH_BASE}-debug.bat" 
	DESTINATION "." 
    COMPONENT ClusterTools)

endif()

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/post-install.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/post-install.py")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mcc_config.py.in" "${CMAKE_CURRENT_BINARY_DIR}/mcc_config_for_install.py")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcc_config_for_install.py"  
  DESTINATION "${MCC_INSTALL_BINDIR}"
  COMPONENT ClusterTools
  RENAME "mcc_config.py")

install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_NAME}"  
  DESTINATION "${MCC_INSTALL_BINDIR}"
  COMPONENT ClusterTools)

install(FILES clusterhost.py remote_clusterhost.py request_handler.py 
  util.py config_parser.py cfg.pem ${CMAKE_CURRENT_BINARY_DIR}/post-install.py DESTINATION "${MCC_INSTALL_SUBDIR}"
  COMPONENT ClusterTools)

install(DIRECTORY tst DESTINATION "${MCC_INSTALL_SUBDIR}"  
  COMPONENT ClusterTools)


# Additional utility targets for MCC development
option(WITH_MCC_DEV "Include MySQL Cluster Configurator dev targets" OFF)
MARK_AS_ADVANCED(WITH_MCC_DEV)
IF (WITH_MCC_DEV)
  MESSAGE(STATUS "Adding MCC dev targets")

  if(WIN32 AND NOT CMAKE_INSTALL_CONFIG_NAME)
    set(CMAKE_INSTALL_CONFIG_NAME $(Configuration))
  endif()

  # Convenience target for mcc development which installs only the mcc files,
  # this is much faster than installing everything
  add_custom_target(mcc_install
	${CMAKE_COMMAND} "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_INSTALL_CONFIG_NAME}" "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" 
	-P "${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake" 
	VERBATIM)
  ADD_DEPENDENCIES(mcc_install host_info)

  # Convenience target for mcc development which runs the mcc tests on installed files
  add_custom_target(mcc_run
	COMMAND ${CMAKE_COMMAND} -E remove mcc_log.txt
        COMMAND python "${CMAKE_INSTALL_PREFIX}/${MCC_INSTALL_BINDIR}/${MCC_SCRIPT_NAME}" -N "" -d DEBUG -o mcc_log.txt VERBATIM)
  add_dependencies(mcc_run mcc_install)

  add_custom_target(mcc_run_in_source python "${MCC_SCRIPT_BASE}_in_source.py" -d DEBUG -o mcc_in_source_log.txt VERBATIM)
  add_custom_target(mcc_punit python "${CMAKE_CURRENT_SOURCE_DIR}/tst/tst_main.py" VERBATIM)
ENDIF()
