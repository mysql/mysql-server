# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE_DIRECTORIES(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/extra/yassl/include
  ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/include
  ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/mySTL
)

ADD_DEFINITIONS(${SSL_DEFINES})

## Do not treat warnings as error for yassl code
STRING(REGEX REPLACE "-Werror( |$)" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

MYSQL_ADD_EXECUTABLE(yassl-t yassl-t.cc ADD_TEST yassl)
ADD_COMPILE_FLAGS(yassl-t.cc COMPILE_FLAGS ${DISABLE_PSI_OPTIONS})
TARGET_LINK_LIBRARIES(yassl-t gunit_small sqlgunitlib strings dbug)
TARGET_LINK_LIBRARIES(yassl-t ${LIBSOCKET})
