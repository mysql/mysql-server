# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/domaintypehandler
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/model
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/framework
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/junit/textui
 ${CMAKE_CURRENT_SOURCE_DIR}/../clusterj-unit/src/main/java/org/junit)

SET ( BROKEN_JAVAC
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/domaintypehandler/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/testsuite/clusterj/model/*.java
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
  ${WITH_CLASSPATH}
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-core/target/classes)

SET(CLUSTERJ_TEST_EXPORTS regression,testsuite.clusterj,testsuite.clusterj.model,testsuite.clusterj.domaintypehandler,junit.framework,junit.textui,org.junit)
CREATE_MANIFEST(manifest.mf ${CLUSTERJ_TEST_EXPORTS} clusterj-test)

CREATE_JAR(clusterj-test ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/manifest.mf
  DEPENDENCIES clusterj.jar
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/schema.sql
  BROKEN_JAVAC ${BROKEN_JAVAC})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-test-${JAVA_NDB_VERSION}.jar
        DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
