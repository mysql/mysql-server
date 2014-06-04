# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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

SET ( JAVA_SOURCE_DIRS
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/jpatest
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/jpatest/model
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/framework
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/textui
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/org/junit)

SET ( BROKEN_JAVAC
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/jpatest/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/jpatest/model/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/framework/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/textui/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/org/junit/*.java)

SET ( JAVA_SOURCES "")
FOREACH(D ${JAVA_SOURCE_DIRS})
  FILE(GLOB tmp ${D}/*.java)
  LIST(APPEND JAVA_SOURCES ${tmp})
ENDFOREACH()

SET ( CLASSPATH 
  target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-test/target/classes
  ${WITH_CLASSPATH})

SET(CLUSTERJ_JPATEST_EXPORTS com.mysql.clusterj.jpatest,com.mysql.clusterj.jpatest.model,junit.framework,junit.textui,org.junit)
CREATE_MANIFEST(manifest.mf ${CLUSTERJ_JPATEST_EXPORTS} clusterj-jpatest)

CREATE_JAR(clusterj-jpatest ${JAVA_SOURCES} 
  CLASSPATH ${CLASSPATH}
  MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.mf
  ENHANCE ${CMAKE_CURRENT_SOURCE_DIR}/src/main/resources/META-INF/persistence.xml
  DEPENDENCIES clusterjpa.jar
  EXTRA_FILES src/main/resources/META-INF
  BROKEN_JAVAC ${BROKEN_JAVAC})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-jpatest-${JAVA_NDB_VERSION}.jar
        DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
