#   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; version 2 of the License.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/java_support.mk.am

clusterj_test_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-test/src/main/java

clusterj_test_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-test/src/main/resources

MANIFEST = MANIFEST.MF

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-test/target/classes

CLASSPATH_ENV = CLASSPATH=$(JAVAROOT):$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/target/classes:@CLUSTERJ_CLASSPATH@

clusterj_testdir= $(prefix)/share/mysql/java/

clusterj_test_version_JAR = clusterj-test-@JAVA_NDB_VERSION@.jar

clusterj_test_DATA = $(clusterj_test_version_JAR) 

clusterj_test_java = \
  $(clusterj_test_src)/regression/Bug54619.java \
  $(clusterj_test_src)/testsuite/clusterj/AbstractClusterJModelTest.java \
  $(clusterj_test_src)/testsuite/clusterj/AbstractClusterJTest.java \
  $(clusterj_test_src)/testsuite/clusterj/AbstractQueryTest.java \
  $(clusterj_test_src)/testsuite/clusterj/AllTests.java \
  $(clusterj_test_src)/testsuite/clusterj/AutoCommitTest.java \
  $(clusterj_test_src)/testsuite/clusterj/BigIntegerTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/BinaryTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/BinaryPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/BitTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/BlobTest.java \
  $(clusterj_test_src)/testsuite/clusterj/CharsetTest.java \
  $(clusterj_test_src)/testsuite/clusterj/ConnectionPoolTest.java \
  $(clusterj_test_src)/testsuite/clusterj/CoordinatedTransactionIdVariableTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DateAsSqlDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DateAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DatetimeAsSqlTimestampTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DatetimeAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DbugTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DecimalTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DefaultConnectValuesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DeleteAllByClassTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DeleteInsertTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DeleteQueryAllPrimitivesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DomainTypeHandlerFactoryTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DynamicBinaryPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/DynamicObjectTest.java \
  $(clusterj_test_src)/testsuite/clusterj/FindByPrimaryKey2Test.java \
  $(clusterj_test_src)/testsuite/clusterj/FindByPrimaryKeyTest.java \
  $(clusterj_test_src)/testsuite/clusterj/HashOnlyLongIntStringPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/LoadTest.java \
  $(clusterj_test_src)/testsuite/clusterj/LongIntStringPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/LongLongStringPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/LongvarbinaryPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/MultiplePKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/MultithreadedFindTest.java \
  $(clusterj_test_src)/testsuite/clusterj/MultithreadedTest.java \
  $(clusterj_test_src)/testsuite/clusterj/NegativeMetadataTest.java \
  $(clusterj_test_src)/testsuite/clusterj/NotPersistentTest.java \
  $(clusterj_test_src)/testsuite/clusterj/NullValuesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/ObjectNotFoundTest.java \
  $(clusterj_test_src)/testsuite/clusterj/PartitionKeyTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryAllPrimitivesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryBigIntegerTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryBtreeIndexScanTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryByteArrayTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDateAsSqlDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDateAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDatetimeAsSqlTimestampTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDatetimeAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDecimalTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryDoubleTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryExplainTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryExtraConditionsTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryFloatTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryHashIndexScanTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryInTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryLikeTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryLikeByteArrayTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryLimitsTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryMultiColumnIndexInTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryMultipleParameterTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryNotNullTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryNotTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryNullTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryOrTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryOrderingTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryPrimaryKeyTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryStringTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryTableScanTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryTimeAsSqlTimeTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryTimeAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryTimestampAsSqlTimestampTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryTimestampAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/QueryYearTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/SaveTest.java \
  $(clusterj_test_src)/testsuite/clusterj/SchemaChangeTest.java \
  $(clusterj_test_src)/testsuite/clusterj/SerialTransactionsTest.java \
  $(clusterj_test_src)/testsuite/clusterj/StressTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TableWithoutPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TimeAsSqlTimeTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TimeAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TimestampAsSqlTimestampTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TimestampAsUtilDateTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/TransactionStateTest.java \
  $(clusterj_test_src)/testsuite/clusterj/UpdateTest.java \
  $(clusterj_test_src)/testsuite/clusterj/VarbinaryPKTest.java \
  $(clusterj_test_src)/testsuite/clusterj/VarbinaryTypesTest.java \
  $(clusterj_test_src)/testsuite/clusterj/domaintypehandler/CrazyDomainTypeHandlerFactoryImpl.java \
  $(clusterj_test_src)/testsuite/clusterj/model/AllPrimitives.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadEmployeeNoPrimaryKeyAnnotationOnClass.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadEmployeePrimaryKeyAnnotationColumnAndColumns.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadEmployeePrimaryKeyAnnotationNoColumnOrColumns.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadEmployeePrimaryKeyAnnotationOnClassMisspelledField.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadEmployeeWrongPrimaryKeyAnnotationOnClass.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadIndexDuplicateColumn.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadIndexDuplicateIndexName.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BadIndexMissingColumn.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BigIntegerTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BinaryTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BinaryPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BitTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/BlobTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/ByteArrayTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CharsetBig5.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CharsetLatin1.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CharsetModel.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CharsetSjis.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CharsetUtf8.java \
  $(clusterj_test_src)/testsuite/clusterj/model/CrazyDelegate.java \
  $(clusterj_test_src)/testsuite/clusterj/model/Customer.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DateAsSqlDateTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DateAsUtilDateTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DatetimeAsSqlTimestampTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DatetimeAsUtilDateTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DecimalTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/Dn2id.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DoubleTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/DynamicPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/Employee.java \
  $(clusterj_test_src)/testsuite/clusterj/model/Employee2.java \
  $(clusterj_test_src)/testsuite/clusterj/model/FloatTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/HashOnlyLongIntStringPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/IdBase.java \
  $(clusterj_test_src)/testsuite/clusterj/model/IndexesRUs.java \
  $(clusterj_test_src)/testsuite/clusterj/model/LongIntStringIndex.java \
  $(clusterj_test_src)/testsuite/clusterj/model/LongIntStringPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/LongLongStringPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/LongvarbinaryPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/NotPersistentTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/NullValues.java \
  $(clusterj_test_src)/testsuite/clusterj/model/Order.java \
  $(clusterj_test_src)/testsuite/clusterj/model/OrderLine.java \
  $(clusterj_test_src)/testsuite/clusterj/model/StringTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/ThrowNullPointerException.java \
  $(clusterj_test_src)/testsuite/clusterj/model/TimeAsSqlTimeTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/TimeAsUtilDateTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/TimestampAsSqlTimestampTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/TimestampAsUtilDateTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/VarbinaryPK.java \
  $(clusterj_test_src)/testsuite/clusterj/model/VarbinaryTypes.java \
  $(clusterj_test_src)/testsuite/clusterj/model/YearTypes.java

EXTRA_DIST = \
  src/main/resources/META-INF \
  src/main/resources/schema.sql \
  $(clusterj_test_java) \
  pom.xml CMakeLists.txt

$(MANIFEST):
	echo "Manifest-Version: 1.0"  > $(MANIFEST)
	echo "Export-Package: regression,testsuite.clusterj,testsuite.clusterj.model,testsuite.clusterj.domaintypehandler"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ Test"  >> $(MANIFEST)
	echo "Bundle-Description: The Test Suite for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-test" >> $(MANIFEST)

classclusterj_test.stamp: $(clusterj_test_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_test.stamp


$(clusterj_test_version_JAR) : classclusterj_test.stamp $(MANIFEST)
	rm -f $@ ; @JAR@ cvfm $@ $(MANIFEST) \
	  -C $(clusterj_test_res) META-INF \
	  -C $(clusterj_test_res) schema.sql \
	  -C $(JAVAROOT) testsuite/clusterj

CLEANFILES= $(clusterj_test_version_JAR) classclusterj_test.stamp

clean-local:
	rm -f $(JAVAROOT)/testsuite/clusterj/*.class \
  $(JAVAROOT)/testsuite/clusterj/model/*.class \
  $(JAVAROOT)/testsuite/clusterj/domaintypehandler/*.class

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-test/target
