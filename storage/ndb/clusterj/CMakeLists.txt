# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

MESSAGE(STATUS "including ClusterJ")

IF("${WITH_CLASSPATH}" MATCHES "junit-(.+).jar")
  MESSAGE ( STATUS "Found junit jar" )
  SET(HAVE_JUNIT TRUE)
ENDIF()

IF("${WITH_CLASSPATH}" MATCHES "openjpa-(.+).jar")
  MESSAGE ( STATUS "Found openjpa jar" )
  SET(HAVE_OPENJPA TRUE)
ENDIF()

SET(DEP ndbjtie.jar
        tie.jar
        clusterj-api.jar
        clusterj-core.jar
        clusterj-tie.jar)

SET(JARS ${CMAKE_CURRENT_BINARY_DIR}/../src/ndbjtie/ndbjtie.jar
         ${CMAKE_CURRENT_BINARY_DIR}/../src/ndbjtie/jtie/jtie.jar
         ${CMAKE_CURRENT_BINARY_DIR}/clusterj-api/clusterj-api.jar
         ${CMAKE_CURRENT_BINARY_DIR}/clusterj-core/clusterj-core.jar
         ${CMAKE_CURRENT_BINARY_DIR}/clusterj-tie/clusterj-tie.jar)

ADD_SUBDIRECTORY(clusterj-api)
ADD_SUBDIRECTORY(clusterj-core)
ADD_SUBDIRECTORY(clusterj-tie)

IF(HAVE_OPENJPA)
  ADD_SUBDIRECTORY(clusterj-openjpa)
  LIST(APPEND DEP clusterjpa.jar)
  LIST(APPEND JARS ${CMAKE_CURRENT_BINARY_DIR}/clusterj-openjpa/clusterjpa.jar)
ENDIF(HAVE_OPENJPA)

IF(HAVE_JUNIT)
  ADD_SUBDIRECTORY(clusterj-test)
  IF(HAVE_OPENJPA)
    ADD_SUBDIRECTORY(clusterj-jpatest)
  ENDIF(HAVE_OPENJPA)
ENDIF(HAVE_JUNIT)

SET(CLUSTERJ_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation,com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie)
CREATE_MANIFEST(clusterj_manifest.mf ${CLUSTERJ_EXPORTS} clusterj)

CREATE_JAR(clusterj
  MERGE_JARS ${JARS}
  MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/clusterj_manifest.mf
  DEPENDENCIES ${DEP} )
