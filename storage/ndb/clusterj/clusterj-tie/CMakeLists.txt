# Copyright (c) 2010, 2022, Oracle and/or its affiliates.
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

SET(CLUSTERJ_TIE_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/tie)

SET(JAVA_SOURCES
  ${CLUSTERJ_TIE_PREFIX}/BlobImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ClusterConnectionImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ClusterConnectionServiceImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ClusterTransactionImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ColumnImpl.java
  ${CLUSTERJ_TIE_PREFIX}/DbImpl.java
  ${CLUSTERJ_TIE_PREFIX}/DbImplForNdbRecord.java
  ${CLUSTERJ_TIE_PREFIX}/DbugImpl.java
  ${CLUSTERJ_TIE_PREFIX}/DictionaryImpl.java
  ${CLUSTERJ_TIE_PREFIX}/FixedByteBufferPoolImpl.java
  ${CLUSTERJ_TIE_PREFIX}/IndexImpl.java
  ${CLUSTERJ_TIE_PREFIX}/IndexOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/IndexScanOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/KeyPart.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordBlobImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordDeleteOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordIndexScanOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordInsertOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordKeyOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordResultDataImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordScanOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordScanResultDataImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordSmartValueHandlerFactoryImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordSmartValueHandlerImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordTableScanOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/NdbRecordUniqueKeyOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/OperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/PartitionKeyImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ResultDataImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ScanFilterImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ScanOperationImpl.java
  ${CLUSTERJ_TIE_PREFIX}/ScanResultDataImpl.java
  ${CLUSTERJ_TIE_PREFIX}/TableImpl.java
  ${CLUSTERJ_TIE_PREFIX}/Utility.java
  ${CLUSTERJ_TIE_PREFIX}/VariableByteBufferPoolImpl.java
  )

SET(CLASSPATH
  target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes
  )

SET(JAR_DEPENDENCIES
  clusterj-core.jar
  ndbjtie.jar
  )

CREATE_JAR(clusterj-tie ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  DEPENDENCIES ${JAR_DEPENDENCIES}
  EXTRA_FILES src/main/resources/META-INF src/main/resources/com
  )
