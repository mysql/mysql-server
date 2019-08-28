# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

MY_CHECK_C_COMPILER_FLAG("-Wstringop-truncation" HAVE_STRINGOP_TRUNCATION)
IF(HAVE_STRINGOP_TRUNCATION)
  ADD_COMPILE_FLAGS(
    main.c
    COMPILE_FLAGS -Wno-stringop-truncation
    )
ENDIF()

SET(REGEX_SOURCES regcomp.c regerror.c regexec.c regfree.c reginit.c)
ADD_CONVENIENCE_LIBRARY(regex ${REGEX_SOURCES})

IF(WITH_UNIT_TESTS)
  ADD_EXECUTABLE(re main.c split.c debug.c)
  TARGET_LINK_LIBRARIES(re regex strings mysys)

  ADD_TEST(regex1 re     -I)
  ADD_TEST(regex2 re -el -I)
  ADD_TEST(regex3 re -er -I)
ENDIF()
