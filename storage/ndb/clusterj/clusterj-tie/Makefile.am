#   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

clusterj_tie_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/src/main/java
clusterj_tie_test = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/src/test/java

clusterj_tie_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/src/main/resources

CLASSPATH_ENV = CLASSPATH=$(JAVAROOT):$(top_srcdir)/storage/ndb/src/ndbjtie/ndbjtie-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/src/ndbjtie/jtie/jtie-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/target/classes

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/target/classes

clusterj_tie_java= \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/Utility.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/BlobImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterConnectionImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterConnectionServiceImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterTransactionImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ColumnImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DbImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DbImplForNdbRecord.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DbugImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DictionaryImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordBlobImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordDeleteOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordIndexScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordInsertOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordKeyOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordScanResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordSmartValueHandlerFactoryImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordSmartValueHandlerImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordTableScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/NdbRecordUniqueKeyOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/OperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanFilterImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/TableImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/KeyPart.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/PartitionKeyImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/KeyPart.java 


noinst_DATA = classclusterj_tie.stamp 

EXTRA_DIST = \
  src/main/resources/META-INF \
  src/test/resources/clusterj.properties \
  src/main/resources/com/mysql/clusterj/tie/Bundle.properties \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/*.java \
  $(clusterj_tie_test)/testsuite/clusterj/tie/*.java \
  pom.xml CMakeLists.txt

classclusterj_tie.stamp: $(clusterj_tie_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_tie.stamp

CLEANFILES= classclusterj_tie.stamp

clean-local:
	rm -rf $(JAVAROOT)/com/mysql/clusterj/tie/*.class

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/target
