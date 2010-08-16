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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/unittest/mytap)
ADD_EXECUTABLE(simple-t simple-t.c)
TARGET_LINK_LIBRARIES(simple-t mytap)

ADD_EXECUTABLE(skip-t skip-t.c)
TARGET_LINK_LIBRARIES(skip-t mytap)

ADD_EXECUTABLE(todo-t todo-t.c)
TARGET_LINK_LIBRARIES(todo-t mytap)

ADD_EXECUTABLE(skip_all-t skip_all-t.c)
TARGET_LINK_LIBRARIES(skip_all-t mytap)

ADD_EXECUTABLE(no_plan-t no_plan-t.c)
TARGET_LINK_LIBRARIES(no_plan-t mytap)

ADD_EXECUTABLE(core-t core-t.c)
TARGET_LINK_LIBRARIES(core-t mytap)
