# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
## Java Crund

# The main program consists of
#   - Abstract classes Load and Driver
#   - Class CrundDriver extends Driver and provides main()
#   - Abstract classes CrundLoad and CrundSLoad extend Load
#   - For the JDBC implementation:
#       * JdbcAB extends CrundLoad
#       * JdbcS extends CrundSLoad
#   - For the NDBJTIE implementation:
#       * NdbjtieAB extends CrundLoad
#       * NdbjtieS extends CrundSLoad
#   - For the Cluster/J implementation:
#       * IA and IB declare interfaces over A and B
#       * ClusterjAB extends CrundLoad
#       * ClusterjS extends CrundSLoad
#
#   JdbcDriverTest is a stand-alone connection test program
#   ResultProcessor is a stand-alone application

SET(JCRUND_SRC ${CMAKE_CURRENT_SOURCE_DIR})

SET(JAVA_CRUND_SOURCES
    ${JCRUND_SRC}/ClusterjAB.java
    ${JCRUND_SRC}/ClusterjS.java
    ${JCRUND_SRC}/CrundDriver.java
    ${JCRUND_SRC}/CrundLoad.java
    ${JCRUND_SRC}/CrundSLoad.java
    ${JCRUND_SRC}/Driver.java
    ${JCRUND_SRC}/IA.java
    ${JCRUND_SRC}/IB.java
    ${JCRUND_SRC}/JdbcAB.java
    ${JCRUND_SRC}/JdbcS.java
    ${JCRUND_SRC}/Load.java
    ${JCRUND_SRC}/NdbjtieAB.java
    ${JCRUND_SRC}/NdbjtieS.java
    ${JCRUND_SRC}/ResultProcessor.java
    ${JCRUND_SRC}/JdbcDriverTest.java
)

SET(CLASSPATH
    ${CMAKE_CURRENT_BINARY_DIR}/target/classes
    ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
    ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
   )

CREATE_JAR(crund
           ${JAVA_CRUND_SOURCES}
           CLASSPATH ${CLASSPATH}
           DEPENDENCIES clusterj-api.jar ndbjtie.jar
          )

