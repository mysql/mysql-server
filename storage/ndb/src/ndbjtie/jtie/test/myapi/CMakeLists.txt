# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# neither wildcards (nor line breaks) for clean targets supported by cmake
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
  "test_myapi.sh.log;test_myapi.cmd.log")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils)

# a C++ API
ADD_CONVENIENCE_LIBRARY(libmyapi myapi.cpp)
#TARGET_LINK_LIBRARIES(libmyapi <dependencies>)

# a program testing the C++ API
ADD_EXECUTABLE(myapi_test myapi_test.cpp)
TARGET_LINK_LIBRARIES(myapi_test libmyapi)

IF(WIN32)

  # build the unit-test script for win
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_myapi.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_myapi.cmd
		 @ONLY NEWLINE_STYLE WIN32)

ENDIF(WIN32)
