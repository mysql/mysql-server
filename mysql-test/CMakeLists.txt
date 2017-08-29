# Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.
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
  IF(NOT INSTALL_LAYOUT MATCHES "RPM")
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/COPYING
      DESTINATION ${INSTALL_DOCREADMEDIR}
      COMPONENT TestReadme
      RENAME COPYING-test
      OPTIONAL
      )
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/LICENSE.mysql
      DESTINATION ${INSTALL_DOCREADMEDIR}
      COMPONENT TestReadme
      RENAME LICENSE.mysql-test
      OPTIONAL
      )
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/README
      DESTINATION ${INSTALL_DOCREADMEDIR}
      COMPONENT TestReadme
      RENAME README-test
      )
  ENDIF()
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


SET(EXP --experimental=collections/default.experimental)

SET(DEBUG_SERVER "")
SET(VS_CONFIG "")

IF(DEBUG_EXTNAME)
  SET(DEBUG_SERVER "--debug-server")
ENDIF()

IF(CMAKE_BUILD_TYPE)
  IF(CMAKE_BUILD_TYPE MATCHES "Debug")
    SET(DEBUG_SERVER "--debug-server")
  ELSE()
    SET(DEBUG_SERVER "")
    IF(CMAKE_GENERATOR MATCHES "Visual Studio" OR
       CMAKE_GENERATOR MATCHES "Xcode")
      SET(VS_CONFIG "--vs-config=${CMAKE_CFG_INTDIR}")
    ENDIF()
  ENDIF()
ENDIF()

# On windows do: 'cmake --build . --config debug --target test-mtr'
# On unix do: 'make test-mtr'
ADD_CUSTOM_TARGET(test-mtr
  COMMAND perl ./mysql-test-run.pl --force --parallel=4 --suite=main
          ${VS_CONFIG} ${DEBUG_SERVER} ${EXP}
)

## We could do something like this, but the 'test' target generated
## by cmake is really for unit testing: all output is muted by default,
## even in case of test failure.
## ADD_EXECUTABLE(perl IMPORTED)
## ADD_TEST(xxx perl ./mysql-test-run.pl --force
##          ${VS_CONFIG} ${DEBUG_SERVER} ${EXP})


# Process .in files with includes in collections/

MACRO(PROCESS_COLLECTION_INCLUDE collin collection)
  FILE(STRINGS ${collin} inlines)
  FOREACH(line ${inlines})
    IF(${line} MATCHES "#include .*")
      STRING(REPLACE "#include " "collections/" incfile ${line})
      FILE(READ ${incfile} contents)
      FILE(APPEND ${collection} "${contents}")
    ELSE()
      FILE(APPEND ${collection} "${line}\n")
    ENDIF()
  ENDFOREACH()
ENDMACRO()

FILE(GLOB infiles "collections/*.in")
FOREACH(collin ${infiles})
  STRING(REPLACE ".in" "" collection ${collin})
  STRING(REPLACE ".in" ".done" colldone ${collin})
  # Only generate file once
  IF(NOT EXISTS ${colldone})
    PROCESS_COLLECTION_INCLUDE(${collin} ${collection})
    FILE(APPEND ${colldone} "${collin}\n")
  ENDIF()
ENDFOREACH()

# With different MAX_INDEXES values, server might behave differently in
# certain cases. 'max_indexes.inc' file should be updated accordingly to
# reflect the current MAX_INDEXES value. This file helps MTR to decide on
# which tests should be skipped.
# NOTE: While committing a patch please make sure that the file is unmodified
# and should show the default MAX_INDEXES (i.e. 64U).
IF (MAX_INDEXES)
  IF(NOT (${MAX_INDEXES} EQUAL 64U))
    FILE(WRITE include/max_indexes.inc
         "# Warning: This is an auto-generated file. Please do not modify it.
--let $max_indexes = ${MAX_INDEXES}\n")
    MESSAGE(STATUS "mysql-test/include/max_indexes.inc adjusted")
  ENDIF()
ENDIF()
