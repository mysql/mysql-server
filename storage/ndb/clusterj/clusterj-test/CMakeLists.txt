# Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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

SET(BROKEN_JAVAC
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/domaintypehandler/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/model/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/util/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/framework/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/textui/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/org/junit/*.java)

SET(CLUSTERJ_JUNIT_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java)
SET(CLUSTERJ_TESTSUITE_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj)
SET(JAVA_SOURCES
  ${CLUSTERJ_TESTSUITE_PREFIX}/AbstractClusterJModelTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AbstractClusterJTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AbstractQueryTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AllTests.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AutoCloseableTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AutoCommitTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/AutoPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/BigIntegerTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/BinaryPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/BinaryTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/BitTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/BlobTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/Bug17200163Test.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/CharsetTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/ConnectionPoolTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/CoordinatedTransactionIdVariableTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DateAsSqlDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DateAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DatetimeAsSqlTimestampTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DatetimeAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DbugTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DecimalTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DefaultConnectValuesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DeleteAllByClassTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DeleteInsertTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DeleteQueryAllPrimitivesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DomainTypeHandlerFactoryTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DynamicBinaryPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DynamicBytePKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DynamicObjectTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DynamicShortPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/DynamicStringPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/FindByPrimaryKey2Test.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/FindByPrimaryKeyErrorHandlingTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/FindByPrimaryKeyTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/FixedLengthCharTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/HashOnlyLongIntStringPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/LoadTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/LongIntStringPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/LongLongStringPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/LongvarbinaryPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/MediumIntegerTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/MediumUnsignedTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/MultiplePKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/MultithreadedFindTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/MultithreadedTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/NegativeMetadataTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/NotPersistentTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/NullValuesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/ObjectNotFoundTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/PartitionKeyTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/ProjectionTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryAllPrimitivesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryBigIntegerTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryBlobTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryBtreeIndexScanTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryByteArrayTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDateAsSqlDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDateAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDatetimeAsSqlTimestampTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDatetimeAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDecimalTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryDoubleTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryExplainTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryExtraConditionsTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryFloatTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryHashIndexScanTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryHashPKScanTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryInTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryLikeByteArrayTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryLikeTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryLimitsTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryMultiColumnIndexInTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryMultipleParameterTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryNotNullTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryNotTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryNullTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryOrTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryOrderingTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryPrimaryKeyTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryScanLockTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryStringTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTableScanTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTextIndexScanTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTimeAsSqlTimeTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTimeAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTimestampAsSqlTimestampTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryTimestampAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryUniqueKeyTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/QueryYearTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/Reconnect2Test.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/ReconnectTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/RecvThreadCPUTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/ReleaseTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/SaveTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/SchemaChangeTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/SerialTransactionsTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/SessionFactoryTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/StressTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TableWithoutPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TimeAsSqlTimeTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TimeAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/Timestamp2AsSqlTimestampTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TimestampAsSqlTimestampTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TimestampAsUtilDateTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TransactionErrorSetPartitionKeyTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/TransactionStateTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/UpdateTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/VarbinaryPKTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/VarbinaryTypesTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/VarcharStringLengthTest.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/domaintypehandler/CrazyDomainTypeHandlerFactoryImpl.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AllPrimitives.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AutoPKBigint.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AutoPKInt.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AutoPKMediumint.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AutoPKSmallint.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/AutoPKTinyint.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadEmployeeNoPrimaryKeyAnnotationOnClass.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadEmployeePrimaryKeyAnnotationColumnAndColumns.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadEmployeePrimaryKeyAnnotationNoColumnOrColumns.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadEmployeePrimaryKeyAnnotationOnClassMisspelledField.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadEmployeeWrongPrimaryKeyAnnotationOnClass.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadIndexDuplicateColumn.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadIndexDuplicateIndexName.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BadIndexMissingColumn.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BigIntegerTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BinaryPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BinaryTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BitTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/BlobTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/ByteArrayTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetBig5.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetLatin1.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetModel.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetSjis.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetSwedishUtf8.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CharsetUtf8.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/ConversationSummary.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/CrazyDelegate.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Customer.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DateAsSqlDateTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DateAsUtilDateTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DatetimeAsSqlTimestampTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DatetimeAsUtilDateTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DecimalTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Dn2id.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DoubleTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DynamicPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/DynamicStringPKs.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Employee.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Employee2.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/FloatTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/HashOnlyLongIntStringPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/IdBase.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/IndexesRUs.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/LongIntStringIndex.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/LongIntStringPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/LongLongStringPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/LongvarbinaryPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/MediumIntegerTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/MediumUnsignedTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/NotPersistentTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/NullValues.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Order.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/OrderLine.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/StringTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/ThrowNullPointerException.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/TimeAsSqlTimeTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/TimeAsUtilDateTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/Timestamp2AsSqlTimestampTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/TimestampAsSqlTimestampTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/TimestampAsUtilDateTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/VarbinaryPK.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/VarbinaryTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/model/YearTypes.java
  ${CLUSTERJ_TESTSUITE_PREFIX}/util/MgmClient.java

  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/AssertionFailedError.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/Test.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/TestCase.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/TestListener.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/TestResult.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/framework/TestSuite.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/textui/ResultPrinter.java
  ${CLUSTERJ_JUNIT_PREFIX}/junit/textui/TestRunner.java
  ${CLUSTERJ_JUNIT_PREFIX}/org/junit/Ignore.java
  ${CLUSTERJ_JUNIT_PREFIX}/org/junit/DebugTest.java
  )

SET(CLASSPATH
  target/classes
  ${WITH_CLASSPATH}
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-core/target/classes)

SET(CLUSTERJ_TEST_EXPORTS
  junit.framework
  junit.textui
  org.junit
  regression
  testsuite.clusterj
  testsuite.clusterj.domaintypehandler
  testsuite.clusterj.model
  testsuite.clusterj.util
  )
CREATE_MANIFEST(manifest.mf "${CLUSTERJ_TEST_EXPORTS}" clusterj-test)

CREATE_JAR(clusterj-test ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.mf
  DEPENDENCIES clusterj.jar
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/schema.sql
  BROKEN_JAVAC ${BROKEN_JAVAC})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-test-${JAVA_NDB_VERSION}.jar
  DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
