#   Copyright (C) 2009 Sun Microsystems Inc.
#   All rights reserved. Use is subject to license terms.
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

clusterj_jpatest_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/src/main/java

clusterj_jpatest_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/src/main/resources

MANIFEST = MANIFEST.MF

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/target/classes

CLASSPATH_ENV = CLASSPATH=$(JAVAROOT):@CLUSTERJ_CLASSPATH@:../clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar

clusterj_jpatestdir= $(prefix)/share/mysql/java/

clusterj_jpatest_version_JAR = clusterj-jpatest-@JAVA_NDB_VERSION@.jar
clusterj_jpatest_JAR = clusterj-jpatest.jar

clusterj_jpatest_DATA = $(clusterj_jpatest_version_JAR)

clusterj_jpatest_java = \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/AbstractJPABaseTest.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/Driver.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/JpaLoad.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/A.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/B0.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/Employee.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/IdBase.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongIntStringConstants.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongIntStringFKOneOne.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongIntStringOid.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongIntStringPKOneOne.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongLongStringConstants.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongLongStringFKManyOne.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongLongStringOid.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongLongStringPK.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/LongLongStringPKOneMany.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/PersistenceTestCase.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/SingleEMFTestCase.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/SingleEMTestCase.java

EXTRA_DIST= \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/*.java \
  $(clusterj_jpatest_src)/com/mysql/clusterj/jpatest/model/*.java \
  $(clusterj_jpatest_res)/META-INF/persistence.xml \
   pom.xml

$(MANIFEST):
	echo "Manifest-Version: 1.0"  > $(MANIFEST)
	echo "Bundle-Name: ClusterJ API"  >> $(MANIFEST)
	echo "Bundle-Description: The API for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-jpatest" >> $(MANIFEST)

classclusterj_jpatest.stamp: $(clusterj_jpatest_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_jpatest.stamp

$(clusterj_jpatest_version_JAR) : classclusterj_jpatest.stamp $(MANIFEST)
	rm -f $@ ; @JAR@ cvfm $@ $(MANIFEST) \
  	  -C $(JAVAROOT) com/mysql/clusterj/jpatest
	$(LN_CP_F) -f $(clusterj_jpatest_version_JAR) $(clusterj_jpatest_JAR)

CLEANFILES= $(clusterj_jpatest_version_JAR) $(clusterj_jpatest_JAR) classclusterj_jpatest.stamp

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_jpatestdir) && \
	$(LN_CP_F) -f $(clusterj_jpatest_version_JAR) $(clusterj_jpatest_JAR)


clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/jpatest/*.class \
  $(JAVAROOT)/com/mysql/clusterj/jpatest/model/*.class


MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/target
