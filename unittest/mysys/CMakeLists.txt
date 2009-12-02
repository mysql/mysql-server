# Copyright (C) 2007 MySQL AB, 2008-2009 Sun Microsystems, Inc
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/unittest/mytap)
ADD_EXECUTABLE(bitmap-t bitmap-t.c)
TARGET_LINK_LIBRARIES(bitmap-t mytap mysys dbug strings)

ADD_EXECUTABLE(base64-t base64-t.c)
TARGET_LINK_LIBRARIES(base64-t mytap mysys dbug strings)

ADD_EXECUTABLE(my_atomic-t my_atomic-t.c)
TARGET_LINK_LIBRARIES(my_atomic-t mytap mysys dbug strings)

ADD_EXECUTABLE(lf-t lf-t.c)
TARGET_LINK_LIBRARIES(lf-t mytap mysys dbug strings)

ADD_EXECUTABLE(my_rdtsc-t my_rdtsc-t.c)
TARGET_LINK_LIBRARIES(my_rdtsc-t mytap mysys dbug strings)

