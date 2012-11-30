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

clusterj_core_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-core/src/main/java

clusterj_core_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-core/src/main/resources

JAVAROOT=$(top_srcdir)/storage/ndb/clusterj/clusterj-core/target/classes

CLASSPATH_ENV=CLASSPATH=$(JAVAROOT):$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar

clusterj_core_java = \
  $(clusterj_core_src)/com/mysql/clusterj/core/CacheManager.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/AbstractDomainFieldHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/AbstractDomainTypeHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/DomainFieldHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/DomainTypeHandlerFactoryImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/DomainTypeHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/IndexHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/InvocationHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/KeyValueHandlerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/AndPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/BetweenPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/CandidateIndexImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/ComparativePredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/EqualPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/GreaterEqualPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/GreaterThanPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/IsNotNullPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/IsNullPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/LessEqualPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/LessThanPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/LikePredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/ParameterImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/PredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/PropertyImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/QueryBuilderImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/QueryDomainTypeImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/QueryExecutionContextImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/QueryImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/InPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/NotPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/OrPredicateImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/SessionFactoryImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/SessionFactoryServiceImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/SessionImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/DomainFieldHandler.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/DomainTypeHandler.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/DomainTypeHandlerFactory.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/QueryExecutionContext.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/SessionSPI.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/SmartValueHandler.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/ValueHandler.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/ValueHandlerFactory.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/ValueHandlerBatching.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/StateManager.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Blob.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ClusterConnection.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ClusterConnectionService.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ClusterTransaction.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Column.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Db.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Dictionary.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Index.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/IndexOperation.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/IndexScanOperation.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Operation.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ResultData.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ScanFilter.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/ScanOperation.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/Table.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/PartitionKey.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/StoreManager.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/TransactionImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/I18NHelper.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/JDK14LoggerFactoryImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/JDK14LoggerImpl.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/Logger.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/LoggerFactory.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/LoggerFactoryService.java 
noinst_DATA = classclusterj_core.stamp

EXTRA_DIST = \
  $(clusterj_core_res)/META-INF \
  $(clusterj_core_res)/com/mysql/clusterj/core/Bundle.properties \
  $(clusterj_core_src)/com/mysql/clusterj/core/*.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/metadata/*.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/query/*.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/spi/*.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/store/*.java \
  $(clusterj_core_src)/com/mysql/clusterj/core/util/*.java \
  pom.xml CMakeLists.txt

classclusterj_core.stamp: $(clusterj_core_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_core.stamp

CLEANFILES= classclusterj_core.stamp

clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/core/*.class \
  $(JAVAROOT)/com/mysql/clusterj/core/metadata/*.class \
  $(JAVAROOT)/com/mysql/clusterj/core/query/*.class \
  $(JAVAROOT)/com/mysql/clusterj/core/spi/*.class \
  $(JAVAROOT)/com/mysql/clusterj/core/store/*.class \
  $(JAVAROOT)/com/mysql/clusterj/core/util/*.class 

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-core/target


