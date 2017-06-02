# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils)

# a C++ API
ADD_CONVENIENCE_LIBRARY(libmyapi myapi.cpp)
# trick: link against c++ lib to generate C++ runtime symbols for this lib,
# which are missing when compiling with CXX=gcc, even though this unit test
# library has no dependencies upon other files:
TARGET_LINK_LIBRARIES(libmyapi ndbgeneral)

# a program testing the C++ API
NDB_ADD_TEST(test_jtie_myapi-t myapi_test.cpp LIBS libmyapi)
