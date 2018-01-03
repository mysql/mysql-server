# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

MESSAGE(STATUS "including ClusterJ")

SET(DEP ndbjtie.jar
        jtie.jar
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

ADD_SUBDIRECTORY(clusterj-test)

SET(CLUSTERJ_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation,com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie)
CREATE_MANIFEST(clusterj_manifest.mf ${CLUSTERJ_EXPORTS} clusterj)

CREATE_JAR(clusterj
  MERGE_JARS ${JARS}
  MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/clusterj_manifest.mf
  DEPENDENCIES ${DEP} )

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-${JAVA_NDB_VERSION}.jar
        DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)

# create maven pom.xml in all subdirectories
SET(CLUSTERJ_PROJECTS api core test tie unit)

IF(WIN32)

  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/pom.xml.in
                 ${CMAKE_CURRENT_BINARY_DIR}/pom.xml
                 @ONLY NEWLINE_STYLE WIN32)

  FOREACH(CLUSTERJ_PROJECT ${CLUSTERJ_PROJECTS}) 
    # build all the pom.xml for windows
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/clusterj-${CLUSTERJ_PROJECT}/pom.xml.in
                   ${CMAKE_CURRENT_BINARY_DIR}/clusterj-${CLUSTERJ_PROJECT}/pom.xml
                   @ONLY NEWLINE_STYLE WIN32)
    # copy logging.properties to all clusterj subprojects
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/logging.properties 
                   ${CMAKE_CURRENT_BINARY_DIR}/clusterj-${CLUSTERJ_PROJECT}/
                   COPYONLY)
  ENDFOREACH(CLUSTERJ_PROJECT in CLUSTERJ_PROJECTS)

  # build the mvn_install_ndbjtie for windows
  #CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mvn_install_ndbjtie.cmd.in
  #               ${CMAKE_CURRENT_SOURCE_DIR}/mvn_install_ndbjtie.cmd
  #               @ONLY NEWLINE_STYLE WIN32)

ELSE(WIN32)

  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/pom.xml.in
                 ${CMAKE_CURRENT_BINARY_DIR}/pom.xml
                 @ONLY NEWLINE_STYLE UNIX)

  FOREACH(CLUSTERJ_PROJECT ${CLUSTERJ_PROJECTS}) 
    # build the pom.xml for *nix
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/clusterj-${CLUSTERJ_PROJECT}/pom.xml.in
                   ${CMAKE_CURRENT_BINARY_DIR}/clusterj-${CLUSTERJ_PROJECT}/pom.xml
                   @ONLY NEWLINE_STYLE UNIX)
    # copy logging.properties to all clusterj subprojects
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/logging.properties 
                   ${CMAKE_CURRENT_BINARY_DIR}/clusterj-${CLUSTERJ_PROJECT}
                   COPYONLY)
  ENDFOREACH(CLUSTERJ_PROJECT in CLUSTERJ_PROJECTS)

  # build the mvn_install_ndbjtie for *nix
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mvn_install_ndbjtie.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/mvn_install_ndbjtie.sh
                 @ONLY NEWLINE_STYLE UNIX)

  ADD_CUSTOM_TARGET(mvn_install_ndbjtie ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/mvn_install_ndbjtie.sh"
    || echo "") # don't break the build if file was deleted)

ENDIF(WIN32)

