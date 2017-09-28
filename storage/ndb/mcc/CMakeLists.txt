# Copyright (c) 2012, 2017 Oracle and/or its affiliates. All rights reserved.
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

# Check if we are running cmake standalone, or as part of Cluster
if(NOT DEFAULT_MYSQL_HOME)
  PROJECT(mcc)
  cmake_minimum_required(VERSION 2.6)
  enable_testing()
endif()

IF(WITHOUT_SERVER)
  option(WITH_MCC "Include MySQL Cluster Configurator" OFF)
ELSE()
  option(WITH_MCC "Include MySQL Cluster Configurator" ON)
ENDIF()

if(NOT WITH_MCC)
  message(STATUS "Skipping mcc")
  return()
endif(NOT WITH_MCC)

# PB2 on win?
set(PB2DRIVE $ENV{PB2DRIVE})
if(WIN32 AND PB2DRIVE AND NOT MCC_PYTHON_TO_BUNDLE)
	message(FATAL_ERROR "MCC_PYTHON_TO_BUNDLE not set when building in PB2 on Windows")
endif() 

function(SETIFUNDEF_ var val)
  if(NOT DEFINED ${var})
    set(${var} "${val}")
  endif(NOT DEFINED ${var})
endfunction(SETIFUNDEF_)

set(MCC_SCRIPT_BASE "ndb_setup")
set(MCC_SCRIPT_NAME "${MCC_SCRIPT_BASE}.py")
set(MCC_SCRIPT_IN "${MCC_SCRIPT_BASE}.py.in")

set(MCC_LAUNCH_BASE "setup")

macro(SHOW_VAR_ var)
  message("${var}=${${var}}")
endmacro(SHOW_VAR_)

set(MCC_BROWSER_START_PAGE "welcome.html")

# Write setup script for running out of the source tree so that
# make install is not required when testing mcc only.
set(MCC_INSTALL_BINDIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MCC_INSTALL_FRONTENDDIR "frontend")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/${MCC_SCRIPT_NAME}" COPYONLY)
if(NOT WIN32)
  execute_process(COMMAND "chmod" "u+x" "${CMAKE_CURRENT_BINARY_DIR}/${MCC_SCRIPT_NAME}")
endif(NOT WIN32)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mcc_config.py.in" "${CMAKE_CURRENT_BINARY_DIR}/mcc_config.py")

if(NOT DEFAULT_MYSQL_HOME)
  # Don't have the MYSQL variables in this case
  set(MCC_INSTALL_BINDIR "bin")
  set(MCC_INSTALL_SUBDIR "share/mcc")
else(NOT DEFAULT_MYSQL_HOME)
  set(MCC_INSTALL_BINDIR "${INSTALL_BINDIR}")
  set(MCC_INSTALL_SUBDIR "${INSTALL_MYSQLSHAREDIR}/mcc")
endif(NOT DEFAULT_MYSQL_HOME)
set(MCC_INSTALL_FRONTENDDIR "${MCC_INSTALL_SUBDIR}/frontend")

add_subdirectory("host_info")
add_subdirectory("frontend")

if(WIN32 AND MCC_PYTHON_TO_BUNDLE AND NOT (MCC_LIGHT_INSTALL AND EXISTS "${CMAKE_INSTALL_PREFIX}/${MCC_INSTALL_SUBDIR}/Python"))
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
      message("${BUNDLEPY_EXE} does not appear to ba a valid executable")
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

endif(WIN32 AND MCC_PYTHON_TO_BUNDLE AND NOT (MCC_LIGHT_INSTALL AND EXISTS "${CMAKE_INSTALL_PREFIX}/${MCC_INSTALL_SUBDIR}/Python"))

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

if(WIN32 AND NOT CMAKE_INSTALL_CONFIG_NAME)
	set(CMAKE_INSTALL_CONFIG_NAME $(Configuration))
endif()

# Convenience target for mcc development which installs only the mcc files,
# this is much faster than installing everything
add_custom_target(install_mcc 
	${CMAKE_COMMAND} "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_INSTALL_CONFIG_NAME}" "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" 
	-P "${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake" 
	VERBATIM)

# Convenience target for mcc development which runs the mcc tests on installed files 
add_custom_target(run_mcc	
	COMMAND ${CMAKE_COMMAND} -E remove mcc_log.txt
        COMMAND python "${CMAKE_INSTALL_PREFIX}/${MCC_INSTALL_BINDIR}/${MCC_SCRIPT_NAME}" -N "" -d DEBUG -o mcc_log.txt VERBATIM)
add_dependencies(run_mcc install_mcc)

add_custom_target(run_mcc_in_source python "${MCC_SCRIPT_BASE}_in_source.py" -d DEBUG -o mcc_in_source_log.txt VERBATIM)
add_custom_target(punit python "${CMAKE_CURRENT_SOURCE_DIR}/tst/tst_main.py" VERBATIM)

if(NOT DEFAULT_MYSQL_HOME)
  if(WIN32)
    SETIFUNDEF_(MCC_CPACK_CMAKE_GENERATOR "Visual Studio 9 2008")
    set(CPACK_CMAKE_GENERATOR "${MCC_CPACK_CMAKE_GENERATOR}")
    SETIFUNDEF_(MCC_CPACK_GENERATOR "ZIP")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    SETIFUNDEF_(MCC_CPACK_GENERATOR "TGZ;RPM")
  else(WIN32)
    SETIFUNDEF_(MCC_CPACK_GENERATOR "TGZ;ZIP")
  endif(WIN32)
  set(CPACK_GENERATOR "${MCC_CPACK_GENERATOR}")

include(CPack)
endif(NOT DEFAULT_MYSQL_HOME)
