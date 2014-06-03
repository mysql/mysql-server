# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/dbug
  ${CMAKE_SOURCE_DIR}/include 
)
SET(DBUG_SOURCES dbug.c)
ADD_CONVENIENCE_LIBRARY(dbug ${DBUG_SOURCES})
TARGET_LINK_LIBRARIES(dbug mysys)

ADD_EXECUTABLE(tests tests.c)
TARGET_LINK_LIBRARIES(tests dbug)

ADD_EXECUTABLE(factorial my_main.c factorial.c)
TARGET_LINK_LIBRARIES(factorial dbug)

IF(NOT WIN32 AND NOT CMAKE_GENERATOR MATCHES Xcode)
  FIND_PROGRAM(GROFF groff)
  FIND_PROGRAM(NROFF nroff)
  MARK_AS_ADVANCED(GROFF)
  MARK_AS_ADVANCED(NROFF)
  SET(OUTPUT_INC output1.r output2.r output3.r output4.r output5.r)
  SET(SOURCE_INC factorial.r main.r example1.r example2.r example3.r)
  ADD_CUSTOM_COMMAND(OUTPUT ${OUTPUT_INC}
                     DEPENDS factorial
                     COMMAND ./factorial 1 2 3 4 5 > output1.r
                     COMMAND ./factorial -\#t:o 2 3 > output2.r
                     COMMAND ./factorial -\#d:t:o 3 > output3.r
                     COMMAND ./factorial -\#d,result:o 4 > output4.r
                     COMMAND ./factorial -\#d:f,factorial:F:L:o 3 > output5.r)
  FOREACH(file ${SOURCE_INC})
    STRING(REGEX REPLACE "\\.r" ".c" srcfile ${file})
    ADD_CUSTOM_COMMAND(OUTPUT ${file} DEPENDS ${srcfile}
                       COMMAND sed -e 's!\\\\!\\\\\\\\!g'
                       <${CMAKE_CURRENT_SOURCE_DIR}/${srcfile} >${file})
  ENDFOREACH(file)
  ADD_CUSTOM_COMMAND(OUTPUT dbug-t DEPENDS tests-t.pl
                     COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/tests-t.pl dbug-t)
  ADD_CUSTOM_TARGET(dbug-unit-tests ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dbug-t)                 
  MY_ADD_TEST(dbug)

  IF(GROFF)
    ADD_CUSTOM_COMMAND(OUTPUT user.ps
                       DEPENDS user.r ${OUTPUT_INC} ${SOURCE_INC}
                       COMMAND ${GROFF} -mm ${CMAKE_CURRENT_SOURCE_DIR}/user.r > user.ps || touch user.ps)
    ADD_CUSTOM_TARGET(ps ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/user.ps)
  ENDIF(GROFF)
  IF(NROFF)
    ADD_CUSTOM_COMMAND(OUTPUT user.t
                       DEPENDS user.r ${OUTPUT_INC} ${SOURCE_INC}
                       COMMAND ${NROFF} -mm ${CMAKE_CURRENT_SOURCE_DIR}/user.r > user.t || touch user.t)
    ADD_CUSTOM_TARGET(t ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/user.t)
  ENDIF(NROFF)

ENDIF()

