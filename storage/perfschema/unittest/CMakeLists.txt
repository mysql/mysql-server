# Copyright (c) 2009, 2010 Sun Microsystems, Inc.
# Use is subject to license terms.
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
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/include/mysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/unittest/mytap
                    ${CMAKE_SOURCE_DIR}/storage/perfschema)

ADD_DEFINITIONS(-DMYSQL_SERVER)

MACRO (PFS_ADD_TEST name)
  ADD_EXECUTABLE(${name}-t ${name}-t.cc)
  TARGET_LINK_LIBRARIES(${name}-t mytap perfschema mysys)
  ADD_TEST(${name} ${name}-t)
ENDMACRO()

SET(tests
 pfs_instr_class
 pfs_instr_class-oom
 pfs_instr
 pfs_instr-oom
 pfs
)
FOREACH(testname ${tests})
  PFS_ADD_TEST(${testname})
ENDFOREACH()
