# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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


SET(MTR_FORCE perl ./mysql-test-run.pl --force)
IF(EXISTS ${CMAKE_SOURCE_DIR}/mysql-test/suite/nist)
  SET(TEST_NIST ${MTR_FORCE} --comment=nist suite=nist ${EXP} && 
    ${MTR_FORCE} --comment=nist --force --suite=nist+ps ${EXP})
ELSE()
  SET(TEST_NIST echo "NIST tests not found")
ENDIF()

IF(WITH_EMBEDDED_SERVER)
 SET(TEST_EMBEDDED  ${MTR_FORCE} --comment=embedded  --timer  --embedded-server 
   --skip-rpl --skip-ndbcluster $(EXP))
ELSE()
 SET(TEST_EMBEDDED echo "Can not test embedded, not compiled in")
ENDIF()

SET(TEST_BT_START
  COMMAND ${SETCONFIG_COMMAND}
  COMMAND ${SETOS_COMMAND}
  COMMAND ${SET_ENV} MTR_BUILD_THREAD=auto
)

ADD_CUSTOM_TARGET(test-force
  ${TEST_BT_START}
  COMMAND ${MTR_FORCE}
)

ADD_CUSTOM_TARGET(test-bt
  ${TEST_BT_START}
  COMMAND ${MTR_FORCE} --comment=normal --timer  --skip-ndbcluster --report-features ${EXP}
  COMMAND ${MTR_FORCE} --comment=ps --timer	 --skip-ndbcluster --ps-protocol ${EXP}
  COMMAND ${MTR_FORCE} --comment=funcs1+ps --ps-protocol --reorder --suite=funcs_1 ${EXP}
  COMMAND ${MTR_FORCE} --comment=funcs2 --suite=funcs_2 ${EXP}
  COMMAND ${MTR_FORCE} --comment=partitions --suite=parts ${EXP}
  COMMAND ${MTR_FORCE} --comment=stress --suite=stress ${EXP}
  COMMAND ${MTR_FORCE} --force --comment=jp --suite=jp ${EXP}
  COMMAND ${TEST_NIST}
  COMMAND ${TEST_EMBEDDED}
)

ADD_CUSTOM_TARGET(test-bt-fast
  ${TEST_BT_START}
  COMMAND ${MTR_FORCE}  --comment=ps  --timer  --skip-ndbcluster --ps-protocol --report-features ${EXP}
  COMMAND ${MTR_FORCE} --comment=stress --suite=stress ${EXP}
)

ADD_CUSTOM_TARGET(test-bt-debug
  ${TEST_BT_START}
  COMMAND ${MTR_FORCE} --comment=debug --timer  --skip-ndbcluster --skip-rpl --report-features ${EXP}
)

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
  # Only generate file once
  IF(NOT EXISTS ${collection})
    PROCESS_COLLECTION_INCLUDE(${collin} ${collection})
  ENDIF()
ENDFOREACH()
