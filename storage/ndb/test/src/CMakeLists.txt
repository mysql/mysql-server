# Copyright (C) 2008, 2018, Oracle and/or its affiliates. All rights reserved
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/test/include
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/debugger)
INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)

ADD_CONVENIENCE_LIBRARY(ndbNDBT
        NDBT_ReturnCodes.cpp
        NDBT_Error.cpp
        NDBT_Tables.cpp
        NDBT_ResultRow.cpp
        NDBT_Test.cpp
        NDBT_Thread.cpp
        NDBT_Find.cpp
        NdbMixRestarter.cpp
        HugoCalculator.cpp
        HugoOperations.cpp
        HugoTransactions.cpp
        HugoAsynchTransactions.cpp
        UtilTransactions.cpp
        NdbRestarter.cpp
        NdbRestarts.cpp
        NDBT_Output.cpp
        NdbBackup.cpp
        NdbConfig.cpp
        NDBT_Table.cpp
        NdbSchemaCon.cpp
        NdbSchemaOp.cpp
        getarg.c
        CpcClient.cpp
	SocketInputStream2.cpp
        AtrtClient.cpp
        DbUtil.cpp
	HugoQueryBuilder.cpp
        HugoQueries.cpp
        NdbHistory.cpp
)
TARGET_LINK_LIBRARIES(ndbNDBT ${LIBM})
