# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_BINARY_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/logger)

# "system" does not work, ndbzio.c includes private, non-existing headers.
IF(WITH_ZLIB STREQUAL "bundled")
  INCLUDE_DIRECTORIES(SYSTEM ${CMAKE_SOURCE_DIR}/extra/zlib)
ENDIF()

ADD_DEFINITIONS(-DNO_DUMMY_DECL)
ADD_CONVENIENCE_LIBRARY(ndbgeneral
            ndbzio.c
            File.cpp
            basename.cpp
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
            parse_mask.cpp
            LogBuffer.cpp
            ProcessInfo.cpp
            OwnProcessInfo.cpp
)
SET(NDBGENERAL_LIBS ndbtrace ${ZLIB_LIBRARY} mysys mysys_ssl)
TARGET_LINK_LIBRARIES(ndbgeneral ${NDBGENERAL_LIBS})

FOREACH(tests BaseString Bitmask SparseBitmask Parser HashMap2 LinkedStack)
  NDB_ADD_TEST("${tests}-t" "${tests}.cpp" LIBS ndbgeneral)
ENDFOREACH(tests)

NDB_ADD_TEST(ndb_version-t version.cpp LIBS ndbgeneral)

FOREACH(tests NdbPack mysql_utils_test NdbSqlUtil LogBuffer)
  NDB_ADD_TEST("${tests}-t" "${tests}.cpp" LIBS ndbgeneral ndbportlib)
ENDFOREACH(tests)
