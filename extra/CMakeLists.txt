# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}/include 
# Following is for perror, in case NDB is compiled in.
${CMAKE_SOURCE_DIR}/storage/ndb/include
${CMAKE_SOURCE_DIR}/storage/ndb/include/util  
${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi)

# Default install component for the files is Server here
SET(MYSQL_INSTALL_COMPONENT Server)

IF(NOT CMAKE_CROSSCOMPILING)
 ADD_EXECUTABLE(comp_err comp_err.c)
 TARGET_LINK_LIBRARIES(comp_err mysys)
ENDIF()

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                   ${PROJECT_BINARY_DIR}/sql/share/english/errmsg.sys
                   COMMAND comp_err
                    --charset=${PROJECT_SOURCE_DIR}/sql/share/charsets 
                    --out-dir=${PROJECT_BINARY_DIR}/sql/share/
                    --header_file=${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_BINARY_DIR}/include/mysqld_ername.h
                    --state_file=${PROJECT_BINARY_DIR}/include/sql_state.h
                    --in_file=${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt
                   DEPENDS ${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/comp_err.c)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS 
                  ${PROJECT_BINARY_DIR}/include/mysqld_error.h
                  ${PROJECT_BINARY_DIR}/sql/share/english/errmsg.sys 
                  ${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt)

MYSQL_ADD_EXECUTABLE(my_print_defaults my_print_defaults.c)
TARGET_LINK_LIBRARIES(my_print_defaults  mysys)

MYSQL_ADD_EXECUTABLE(perror perror.c)
ADD_DEPENDENCIES(perror GenError)
TARGET_LINK_LIBRARIES(perror mysys)

MYSQL_ADD_EXECUTABLE(resolveip resolveip.c)
TARGET_LINK_LIBRARIES(resolveip mysys)
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  INCLUDE(CheckFunctionExists)
  INCLUDE(CheckLibraryExists)
  MY_SEARCH_LIBS(inet_aton  "nsl;socket;resolv" SOLARIS_NSL)
  TARGET_LINK_LIBRARIES(resolveip ${SOLARIS_NSL})
ENDIF()


MYSQL_ADD_EXECUTABLE(replace replace.c)
TARGET_LINK_LIBRARIES(replace mysys)
IF(UNIX)
  MYSQL_ADD_EXECUTABLE(innochecksum innochecksum.c)

  MYSQL_ADD_EXECUTABLE(resolve_stack_dump resolve_stack_dump.c)
  TARGET_LINK_LIBRARIES(resolve_stack_dump mysys)

  MYSQL_ADD_EXECUTABLE(mysql_waitpid mysql_waitpid.c)
  TARGET_LINK_LIBRARIES(mysql_waitpid mysys)
ENDIF()

