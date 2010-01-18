# Copyright (C) 2009 Sun Microsystems, Inc
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
                    ${CMAKE_SOURCE_DIR}/include/mysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/unittest/mytap
                    ${CMAKE_SOURCE_DIR}/storage/perfschema)

ADD_DEFINITIONS(-DMYSQL_SERVER)

LINK_LIBRARIES(perfschema mytap mysys dbug strings)

ADD_EXECUTABLE(pfs_instr_class-t pfs_instr_class-t.cc)

ADD_EXECUTABLE(pfs_instr_class-oom-t pfs_instr_class-oom-t.cc)

ADD_EXECUTABLE(pfs_instr-t pfs_instr-t.cc)

ADD_EXECUTABLE(pfs_instr-oom-t pfs_instr-oom-t.cc)

ADD_EXECUTABLE(pfs_timer-t pfs_timer-t.cc)

ADD_EXECUTABLE(pfs-t pfs-t.cc)

