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

INCLUDE_DIRECTORIES(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/unittest/gunit
  ${CMAKE_SOURCE_DIR}/extra/yassl/include
  ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/include
  ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/mySTL
)

ADD_DEFINITIONS(${SSL_DEFINES})

## Do not treat warnings as error for yassl code
STRING(REPLACE "-Werror" "" nowerr_flags "${CMAKE_CXX_FLAGS}")
SET(CMAKE_CXX_FLAGS ${nowerr_flags})

ADD_EXECUTABLE(yassl-t yassl-t.cc)
TARGET_LINK_LIBRARIES(yassl-t gunit_small sqlgunitlib strings dbug regex)
TARGET_LINK_LIBRARIES(yassl-t ${LIBSOCKET})
