# Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
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
#

ADD_DEFINITIONS(-DSTANDALONE_BINLOG)

# this will be moved to the top level CMakeLists(outside libbinlogevents)
# in the independent repo mysql-binlog
# Options for building
#OPTION(WITH_SERVER_TESTS
#       "Build the unit test suite with tests requiring a server" OFF)

#IF(GTEST_FOUND)
#  ENABLE_TESTING(TRUE)
#  INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIRS})
#ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/libbinlogevents/include
                    ${CMAKE_CURRENT_BINARY_DIR}/../libbinlogevents/include
                    ${CMAKE_SOURCE_DIR}/libbinlogevents/export
                    ${CMAKE_CURRENT_BINARY_DIR}/libbinlogevents/export)

ADD_SUBDIRECTORY(src)

# Configure installation
INSTALL(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h")
