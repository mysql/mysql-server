# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
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

IF(INSTALL_MYSQLTESTDIR)
INSTALL(
  DIRECTORY .
  DESTINATION ${INSTALL_MYSQLTESTDIR}
  USE_SOURCE_PERMISSIONS
  COMPONENT Test
  USE_SOURCE_PERMISSIONS
  PATTERN "var/" EXCLUDE
  PATTERN "lib/My/SafeProcess" EXCLUDE
  PATTERN "lib/t*" EXCLUDE
  PATTERN "CPack" EXCLUDE
  PATTERN "CMake*" EXCLUDE
  PATTERN "mtr.out*" EXCLUDE
  PATTERN ".cvsignore" EXCLUDE
  PATTERN "*.am" EXCLUDE
  PATTERN "*.in" EXCLUDE
)
ENDIF()



IF(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  # Enable running mtr from build directory
  CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/mtr.out-of-source 
    ${CMAKE_CURRENT_BINARY_DIR}/mysql-test-run.pl
    @ONLY
  )
ENDIF()
IF(UNIX)
  EXECUTE_PROCESS(
    COMMAND chmod +x  mysql-test-run.pl
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
     ./mysql-test-run.pl mtr
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
     ./mysql-test-run.pl mysql-test-run
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  ) 
  IF(INSTALL_MYSQLTESTDIR)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mtr 
      ${CMAKE_CURRENT_BINARY_DIR}/mysql-test-run 
      DESTINATION ${INSTALL_MYSQLTESTDIR}
      COMPONENT Test)
  ENDIF()
ENDIF()

IF(CMAKE_GENERATOR MATCHES "Visual Studio")
  SET(SETCONFIG_COMMAND set MTR_VS_CONFIG=${CMAKE_CFG_INTDIR})
ELSEIF(CMAKE_GENERATOR MATCHES "Xcode")
  SET(SETCONFIG_COMMAND export MTR_VS_CONFIG=${CMAKE_CFG_INTDIR})
ELSE()
  SET(SETCONFIG_COMMAND echo Running tests)
ENDIF()
IF(CYGWIN)
  # On cygwin, pretend to be "Unix" system
  SET(SETOS_COMMAND export MTR_CYGWIN_IS_UNIX=1)
ELSE()
  SET(SETOS_COMMAND echo OS=${CMAKE_SYSTEM_NAME})
ENDIF()



SET(EXP --experimental=collections/default.experimental)
IF(WIN32)
  SET(SET_ENV set)
ELSE()
  SET(SET_ENV export)
ENDIF()

SET(TEST_BT_START
  COMMAND ${SETCONFIG_COMMAND}
  COMMAND ${SETOS_COMMAND}
  COMMAND ${SET_ENV} MTR_BUILD_THREAD=auto
)

ADD_CUSTOM_TARGET(test-force
  ${TEST_BT_START}
  COMMAND perl ./mysql-test-run.pl --force ${EXP}
)

FOREACH(collection test-bt test-bt-fast test-bt-debug)
  IF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/collections/${collection})
    MESSAGE(FATAL_ERROR 
      "${CMAKE_CURRENT_SOURCE_DIR}/collections/${collection} does not exist")
  ENDIF()
  SET(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/collections/${collection})
  CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/cmake/run_collection.cmake.in 
    ${CMAKE_CURRENT_BINARY_DIR}/run_collection_${collection}.cmake @ONLY)
  ADD_CUSTOM_TARGET(
    ${collection}
    COMMAND ${TEST_BT_START}
    COMMAND ${CMAKE_COMMAND} -P run_collection_${collection}.cmake
  )
ENDFOREACH()
