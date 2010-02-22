# Copyright (C) 2007 MySQL AB, 2009 Sun Microsystems,Inc
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/unittest/mytap)


MACRO (MY_ADD_TEST name)
  ADD_EXECUTABLE(${name}-t ${name}-t.c)
  TARGET_LINK_LIBRARIES(${name}-t mytap mysys)
  ADD_TEST(${name} ${name}-t)
ENDMACRO()


FOREACH(testname bitmap base64 my_vsnprintf my_atomic my_rdtsc lf)
  MY_ADD_TEST(${testname})
ENDFOREACH()
