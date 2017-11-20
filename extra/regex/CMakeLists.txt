# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(REGEX_SOURCES regcomp.c regerror.c regexec.c regfree.c reginit.c)
ADD_CONVENIENCE_LIBRARY(regex ${REGEX_SOURCES})

IF(WITH_UNIT_TESTS)
  MYSQL_ADD_EXECUTABLE(regex_main main.c split.c debug.c ADD_TEST regex_main)
  TARGET_LINK_LIBRARIES(regex_main regex strings mysys)
ENDIF()
