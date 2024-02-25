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

SET(CLUSTERJ_CORE_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core)

SET ( BROKEN_JAVAC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/metadata/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/query/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/spi/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/store/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/util/*.java)

SET(JAVA_SOURCES
  ${CLUSTERJ_CORE_PREFIX}/CacheManager.java
  ${CLUSTERJ_CORE_PREFIX}/SessionFactoryImpl.java
  ${CLUSTERJ_CORE_PREFIX}/SessionFactoryServiceImpl.java
  ${CLUSTERJ_CORE_PREFIX}/SessionImpl.java
  ${CLUSTERJ_CORE_PREFIX}/StateManager.java
  ${CLUSTERJ_CORE_PREFIX}/StoreManager.java
  ${CLUSTERJ_CORE_PREFIX}/TransactionImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/AbstractDomainFieldHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/AbstractDomainTypeHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/DomainFieldHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/DomainTypeHandlerFactoryImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/DomainTypeHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/IndexHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/InvocationHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/metadata/KeyValueHandlerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/AndPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/BetweenPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/CandidateIndexImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/ComparativePredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/EqualPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/GreaterEqualPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/GreaterThanPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/InPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/IsNotNullPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/IsNullPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/LessEqualPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/LessThanPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/LikePredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/NotPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/OrPredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/ParameterImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/PredicateImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/PropertyImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/QueryBuilderImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/QueryDomainTypeImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/QueryExecutionContextImpl.java
  ${CLUSTERJ_CORE_PREFIX}/query/QueryImpl.java
  ${CLUSTERJ_CORE_PREFIX}/spi/DomainFieldHandler.java
  ${CLUSTERJ_CORE_PREFIX}/spi/DomainTypeHandler.java
  ${CLUSTERJ_CORE_PREFIX}/spi/DomainTypeHandlerFactory.java
  ${CLUSTERJ_CORE_PREFIX}/spi/QueryExecutionContext.java
  ${CLUSTERJ_CORE_PREFIX}/spi/SessionSPI.java
  ${CLUSTERJ_CORE_PREFIX}/spi/SmartValueHandler.java
  ${CLUSTERJ_CORE_PREFIX}/spi/ValueHandler.java
  ${CLUSTERJ_CORE_PREFIX}/spi/ValueHandlerBatching.java
  ${CLUSTERJ_CORE_PREFIX}/spi/ValueHandlerFactory.java
  ${CLUSTERJ_CORE_PREFIX}/store/Blob.java
  ${CLUSTERJ_CORE_PREFIX}/store/ClusterConnection.java
  ${CLUSTERJ_CORE_PREFIX}/store/ClusterConnectionService.java
  ${CLUSTERJ_CORE_PREFIX}/store/ClusterTransaction.java
  ${CLUSTERJ_CORE_PREFIX}/store/Column.java
  ${CLUSTERJ_CORE_PREFIX}/store/Db.java
  ${CLUSTERJ_CORE_PREFIX}/store/Dictionary.java
  ${CLUSTERJ_CORE_PREFIX}/store/Index.java
  ${CLUSTERJ_CORE_PREFIX}/store/IndexOperation.java
  ${CLUSTERJ_CORE_PREFIX}/store/IndexScanOperation.java
  ${CLUSTERJ_CORE_PREFIX}/store/Operation.java
  ${CLUSTERJ_CORE_PREFIX}/store/PartitionKey.java
  ${CLUSTERJ_CORE_PREFIX}/store/ResultData.java
  ${CLUSTERJ_CORE_PREFIX}/store/ScanFilter.java
  ${CLUSTERJ_CORE_PREFIX}/store/ScanOperation.java
  ${CLUSTERJ_CORE_PREFIX}/store/Table.java
  ${CLUSTERJ_CORE_PREFIX}/util/DeMinimisLogger.java
  ${CLUSTERJ_CORE_PREFIX}/util/DeMinimisLoggerFactory.java
  ${CLUSTERJ_CORE_PREFIX}/util/I18NHelper.java
  ${CLUSTERJ_CORE_PREFIX}/util/JDK14LoggerFactoryImpl.java
  ${CLUSTERJ_CORE_PREFIX}/util/JDK14LoggerImpl.java
  ${CLUSTERJ_CORE_PREFIX}/util/Logger.java
  ${CLUSTERJ_CORE_PREFIX}/util/LoggerFactory.java
  ${CLUSTERJ_CORE_PREFIX}/util/LoggerFactoryService.java
  )

SET(CLASSPATH
  target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes)

CREATE_JAR(clusterj-core ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  DEPENDENCIES clusterj-api.jar
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/com
  BROKEN_JAVAC ${BROKEN_JAVAC})
