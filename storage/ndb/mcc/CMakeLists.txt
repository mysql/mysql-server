# Copyright (c) 2012, 2013 Oracle and/or its affiliates. All rights reserved.
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

macro(SETIFUNDEF_ var val)
  if(NOT DEFINED ${var})
    set(${var} "${val}")
  endif(NOT DEFINED ${var})
endmacro(SETIFUNDEF_)

set(MCC_SCRIPT_BASE "ndb_setup")
set(MCC_SCRIPT_NAME "${MCC_SCRIPT_BASE}.py")
set(MCC_SCRIPT_IN "${MCC_SCRIPT_BASE}.py.in")

# Write the script to the source tree (without .py ext) for so that
# make install is not required when testing. We do it here before 
# MCC_INSTALL_X have been set
set(MCC_INSTALL_SUBDIR ".")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_IN}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_NAME}")

set(MCC_LAUNCH_BASE "setup")

# Check if we are running cmake standalone, or as part of Cluster
if(NOT DEFAULT_MYSQL_HOME)
  PROJECT(mcc)
  cmake_minimum_required(VERSION 2.6)
  enable_testing()

  set(WITH_MCC_DEFAULT "ON")
  set(MCC_INSTALL_SUBDIR ".")
  set(MCC_INSTALL_BINDIR "bin")

  SETIFUNDEF_(MCC_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")
  set(CMAKE_INSTALL_PREFIX "${MCC_INSTALL_PREFIX}")
else(NOT DEFAULT_MYSQL_HOME)
  set(WITH_MCC_DEFAULT "ON") # For testing in PB2
  set(MCC_INSTALL_SUBDIR "${INSTALL_MYSQLSHAREDIR}/mcc")
  set(MCC_INSTALL_BINDIR "${INSTALL_BINDIR}")
endif(NOT DEFAULT_MYSQL_HOME)

option(WITH_MCC "Include MySQL Cluster Configurator" ${WITH_MCC_DEFAULT})

if(NOT WITH_MCC)
  message(STATUS "Skipping mcc")
  return()
endif(NOT WITH_MCC) 

macro(SHOW_VAR_ var)
  message("${var}=${${var}}")
endmacro(SHOW_VAR_)

add_subdirectory("host_info")
add_subdirectory("frontend")

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

endif(WIN32 AND MCC_PYTHON_TO_BUNDLE)

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/post-install.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/post-install.py")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${MCC_SCRIPT_IN}" 
  "${CMAKE_CURRENT_BINARY_DIR}/${MCC_SCRIPT_NAME}")

add_custom_target(punit python "${CMAKE_CURRENT_SOURCE_DIR}/tst/tst_main.py" VERBATIM)


install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/${MCC_SCRIPT_NAME}"  
  DESTINATION "${MCC_INSTALL_BINDIR}"
  COMPONENT ClusterTools)

# Install script without .py extension
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/${MCC_SCRIPT_NAME}"  
  DESTINATION "${MCC_INSTALL_BINDIR}" RENAME "${MCC_SCRIPT_BASE}"
  COMPONENT ClusterTools)

if(NOT WIN32)
  execute_process(COMMAND "chmod" "u+x" "${MCC_SCRIPT_BASE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif(NOT WIN32)

install(FILES clusterhost.py remote_clusterhost.py request_handler.py 
  util.py config_parser.py cfg.pem ${CMAKE_CURRENT_BINARY_DIR}/post-install.py DESTINATION "${MCC_INSTALL_SUBDIR}"
  COMPONENT ClusterTools)

install(DIRECTORY tst DESTINATION "${MCC_INSTALL_SUBDIR}"  
  COMPONENT ClusterTools)

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
