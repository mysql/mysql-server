# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_BINARY_DIR}/include
                    ${CMAKE_BINARY_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/mysys
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/logger)

ADD_DEFINITIONS(-DNO_DUMMY_DECL)
ADD_CONVENIENCE_LIBRARY(ndbgeneral
            ndbzio.c
            File.cpp
            md5_hash.cpp
            Properties.cpp
            socket_io.cpp
            SimpleProperties.cpp
            Parser.cpp
            InputStream.cpp
            SocketServer.cpp
            SocketClient.cpp
            SocketAuthenticator.cpp
            OutputStream.cpp
            NdbOut.cpp
            BaseString.cpp
            NdbSqlUtil.cpp
            uucode.c
            random.c
            ndb_rand.c
            version.cpp
            strdup.c
            ConfigValues.cpp
            ndb_init.cpp
            ndb_opts.cpp
            basestring_vsnprintf.c
            Bitmask.cpp
	    SparseBitmask.cpp
            require.c
            Vector.cpp
            NdbPack.cpp
            HashMap2.cpp
            LinkedStack.cpp
            decimal_utils.cpp
            CharsetMap.cpp
            CharsetMapImpl.cpp
)
SET(NDBGENERAL_LIBS ndbtrace ${ZLIB_LIBRARY} mysys)
# Some parts of mysys has been split into it's own library
# called mysys_ssl, existence of new library is indicated
# by new include file my_default.h detected in ndb_configure.cmake
IF(HAVE_MY_DEFAULT_H)
  LIST(APPEND NDBGENERAL_LIBS mysys_ssl) 
ENDIF()
TARGET_LINK_LIBRARIES(ndbgeneral ${NDBGENERAL_LIBS})

ADD_EXECUTABLE(BaseString-t BaseString.cpp)
SET_TARGET_PROPERTIES(BaseString-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_BASE_STRING")
TARGET_LINK_LIBRARIES(BaseString-t ndbgeneral)

ADD_EXECUTABLE(Bitmask-t Bitmask.cpp)
SET_TARGET_PROPERTIES(Bitmask-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_BITMASK")
TARGET_LINK_LIBRARIES(Bitmask-t ndbgeneral)

ADD_EXECUTABLE(SparseBitmask-t SparseBitmask.cpp)
SET_TARGET_PROPERTIES(SparseBitmask-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_SPARSEBITMASK")
TARGET_LINK_LIBRARIES(SparseBitmask-t ndbgeneral)

ADD_EXECUTABLE(Parser-t Parser.cpp)
SET_TARGET_PROPERTIES(Parser-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_PARSER")
TARGET_LINK_LIBRARIES(Parser-t ndbgeneral)

ADD_EXECUTABLE(ndb_version-t version.cpp)
SET_TARGET_PROPERTIES(ndb_version-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_VERSION")
TARGET_LINK_LIBRARIES(ndb_version-t ndbgeneral)

ADD_EXECUTABLE(NdbPack-t NdbPack.cpp)
SET_TARGET_PROPERTIES(NdbPack-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_NDB_PACK")
TARGET_LINK_LIBRARIES(NdbPack-t ndbgeneral ndbportlib)

ADD_EXECUTABLE(HashMap2-t HashMap2.cpp)
SET_TARGET_PROPERTIES(HashMap2-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_HASHMAP2")
TARGET_LINK_LIBRARIES(HashMap2-t ndbgeneral)

ADD_EXECUTABLE(LinkedStack-t LinkedStack.cpp)
SET_TARGET_PROPERTIES(LinkedStack-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_LINKEDSTACK")
TARGET_LINK_LIBRARIES(LinkedStack-t ndbgeneral)

ADD_EXECUTABLE(NdbSqlUtil-t NdbSqlUtil.cpp)
SET_TARGET_PROPERTIES(NdbSqlUtil-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_NDB_SQL_UTIL")
TARGET_LINK_LIBRARIES(NdbSqlUtil-t ndbgeneral ndbportlib)
