# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/test/include
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/debugger)

ADD_LIBRARY(ndbNDBT STATIC
  AtrtClient.cpp
  CpcClient.cpp
  DbUtil.cpp
  HugoAsynchTransactions.cpp
  HugoCalculator.cpp
  HugoOperations.cpp
  HugoQueries.cpp
  HugoQueryBuilder.cpp
  HugoTransactions.cpp
  NDBT_Error.cpp
  NDBT_Find.cpp
  NDBT_Output.cpp
  NDBT_ResultRow.cpp
  NDBT_ReturnCodes.cpp
  NDBT_Table.cpp
  NDBT_Tables.cpp
  NDBT_Test.cpp
  NDBT_Thread.cpp
  NdbBackup.cpp
  NdbConfig.cpp
  NdbHistory.cpp
  NdbMixRestarter.cpp
  NdbRestarter.cpp
  NdbRestarts.cpp
  NdbSchemaCon.cpp
  NdbSchemaOp.cpp
  SocketInputStream2.cpp
  UtilTransactions.cpp
  getarg.cpp
)
TARGET_LINK_LIBRARIES(ndbNDBT ${LIBM})
