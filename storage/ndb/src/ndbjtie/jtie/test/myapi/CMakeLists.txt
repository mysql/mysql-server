# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# neither wildcards (nor line breaks) for clean targets supported by cmake
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
  "test_myapi.sh.log;test_myapi.cmd.log")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils)

# a C++ API
ADD_CONVENIENCE_LIBRARY(libmyapi myapi.cpp)
# trick: link against c++ lib to generate C++ runtime symbols for this lib,
# which are missing when compiling with CXX=gcc, even though this unit test
# library has no dependencies upon other files:
TARGET_LINK_LIBRARIES(libmyapi ndbgeneral)
# no luck with tricks like creating/linking dummy.cc file, neither with:
#SET_TARGET_PROPERTIES(libmyapi PROPERTIES HAS_CXX TRUE)

# a program testing the C++ API
ADD_EXECUTABLE(myapi_test myapi_test.cpp)
TARGET_LINK_LIBRARIES(myapi_test libmyapi)

IF(WIN32)

  # build the unit-test script for win
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_myapi.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_myapi.cmd
		 @ONLY NEWLINE_STYLE WIN32)

ELSE(WIN32)

  # build the unit-test script for *nix
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_myapi.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_myapi.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_myapi ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_myapi.sh"
    || echo "") # don't break the build if file was deleted)

ENDIF(WIN32)
