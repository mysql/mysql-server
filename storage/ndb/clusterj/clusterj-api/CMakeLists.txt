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

SET(CLUSTERJ_API_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj)

SET(JAVA_SOURCES
  ${CLUSTERJ_API_PREFIX}/ClusterJDatastoreException.java
  ${CLUSTERJ_API_PREFIX}/ClusterJException.java
  ${CLUSTERJ_API_PREFIX}/ClusterJFatalException.java
  ${CLUSTERJ_API_PREFIX}/ClusterJFatalInternalException.java
  ${CLUSTERJ_API_PREFIX}/ClusterJFatalUserException.java
  ${CLUSTERJ_API_PREFIX}/ClusterJHelper.java
  ${CLUSTERJ_API_PREFIX}/ClusterJUserException.java
  ${CLUSTERJ_API_PREFIX}/ColumnMetadata.java
  ${CLUSTERJ_API_PREFIX}/ColumnType.java
  ${CLUSTERJ_API_PREFIX}/Constants.java
  ${CLUSTERJ_API_PREFIX}/Dbug.java
  ${CLUSTERJ_API_PREFIX}/DynamicObject.java
  ${CLUSTERJ_API_PREFIX}/DynamicObjectDelegate.java
  ${CLUSTERJ_API_PREFIX}/LockMode.java
  ${CLUSTERJ_API_PREFIX}/Query.java
  ${CLUSTERJ_API_PREFIX}/Results.java
  ${CLUSTERJ_API_PREFIX}/Session.java
  ${CLUSTERJ_API_PREFIX}/SessionFactory.java
  ${CLUSTERJ_API_PREFIX}/SessionFactoryService.java
  ${CLUSTERJ_API_PREFIX}/Transaction.java
  ${CLUSTERJ_API_PREFIX}/annotation/Column.java
  ${CLUSTERJ_API_PREFIX}/annotation/Columns.java
  ${CLUSTERJ_API_PREFIX}/annotation/Extension.java
  ${CLUSTERJ_API_PREFIX}/annotation/Extensions.java
  ${CLUSTERJ_API_PREFIX}/annotation/Index.java
  ${CLUSTERJ_API_PREFIX}/annotation/Indices.java
  ${CLUSTERJ_API_PREFIX}/annotation/Lob.java
  ${CLUSTERJ_API_PREFIX}/annotation/NotPersistent.java
  ${CLUSTERJ_API_PREFIX}/annotation/NullValue.java
  ${CLUSTERJ_API_PREFIX}/annotation/PartitionKey.java
  ${CLUSTERJ_API_PREFIX}/annotation/PersistenceCapable.java
  ${CLUSTERJ_API_PREFIX}/annotation/PersistenceModifier.java
  ${CLUSTERJ_API_PREFIX}/annotation/Persistent.java
  ${CLUSTERJ_API_PREFIX}/annotation/PrimaryKey.java
  ${CLUSTERJ_API_PREFIX}/annotation/Projection.java
  ${CLUSTERJ_API_PREFIX}/query/Predicate.java
  ${CLUSTERJ_API_PREFIX}/query/PredicateOperand.java
  ${CLUSTERJ_API_PREFIX}/query/QueryBuilder.java
  ${CLUSTERJ_API_PREFIX}/query/QueryDefinition.java
  ${CLUSTERJ_API_PREFIX}/query/QueryDomainType.java
  )

SET(CLUSTERJ_API_EXPORTS
  com.mysql.clusterj
  com.mysql.clusterj.annotation
  com.mysql.clusterj.query
  )
CREATE_MANIFEST(manifest.mf "${CLUSTERJ_API_EXPORTS}" clusterj-api)

CREATE_JAR(clusterj-api ${JAVA_SOURCES}
  CLASSPATH target/classes
  MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.mf)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-api-${JAVA_NDB_VERSION}.jar
  DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
