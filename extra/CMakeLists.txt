# Copyright (C) 2006 MySQL AB
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
INCLUDE("${PROJECT_SOURCE_DIR}/win/mysql_manifest.cmake")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

ADD_EXECUTABLE(comp_err comp_err.c)
TARGET_LINK_LIBRARIES(comp_err debug dbug mysys strings zlib wsock32)

GET_TARGET_PROPERTY(COMP_ERR_EXE comp_err LOCATION)

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
                   COMMAND ${COMP_ERR_EXE}
                    --charset=${PROJECT_SOURCE_DIR}/sql/share/charsets 
                    --out-dir=${PROJECT_SOURCE_DIR}/sql/share/
                    --header_file=${PROJECT_SOURCE_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_SOURCE_DIR}/include/mysqld_ername.h
                    --state_file=${PROJECT_SOURCE_DIR}/include/sql_state.h
                    --in_file=${PROJECT_SOURCE_DIR}/sql/share/errmsg.txt
                   DEPENDS comp_err ${PROJECT_SOURCE_DIR}/sql/share/errmsg.txt)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS ${PROJECT_SOURCE_DIR}/include/mysqld_error.h)

ADD_EXECUTABLE(my_print_defaults my_print_defaults.c)
TARGET_LINK_LIBRARIES(my_print_defaults strings mysys debug dbug taocrypt wsock32)

ADD_EXECUTABLE(perror perror.c)
TARGET_LINK_LIBRARIES(perror strings mysys debug dbug wsock32)

ADD_EXECUTABLE(resolveip resolveip.c)
TARGET_LINK_LIBRARIES(resolveip strings mysys debug dbug wsock32)

ADD_EXECUTABLE(replace replace.c)
TARGET_LINK_LIBRARIES(replace strings mysys debug dbug wsock32)

IF(EMBED_MANIFESTS)
  MYSQL_EMBED_MANIFEST("myTest" "asInvoker")
ENDIF(EMBED_MANIFESTS)
