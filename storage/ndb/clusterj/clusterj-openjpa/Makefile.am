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

clusterj_openjpa_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/src/main/java

clusterj_openjpa_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/src/main/resources

clusterj_openjpa_test = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/src/test/java

clusterj_openjpa_test_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/src/test/resources

MANIFEST = MANIFEST.MF

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/target/classes

CLASSPATH_ENV = CLASSPATH=@CLUSTERJ_CLASSPATH@:$(JAVAROOT):$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/clusterj-core-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/clusterj-jpatest-@JAVA_NDB_VERSION@.jar

clusterj_openjpadir= $(prefix)/share/mysql/java/

clusterj_openjpa_version_JAR = clusterj-openjpa-@JAVA_NDB_VERSION@.jar
clusterj_openjpa_JAR = clusterj-openjpa.jar

clusterj_openjpa_java = \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAResult.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPADomainFieldHandlerImpl.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAConfiguration.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAConfigurationImpl.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPADomainTypeHandlerImpl.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAStoreManager.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAValueHandler.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPABrokerFactory.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAProductDerivation.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAStoreQuery.java \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAUtility.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/BasicTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/BigIntegerTypesTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/BlobTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/ClobTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/CrundTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/DateAsSqlDateTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/DateAsUtilDateTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/DatetimeAsSqlTimestampTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/DatetimeAsUtilDateTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/DecimalTypesTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/EmbeddedTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/LongIntStringPKOneOneTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/LongLongStringPKOneManyTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/LongLongStringPKTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/OneToManyRelationshipTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/TestBadPersistenceUnitNoConnectString.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/TimeAsSqlTimeTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/TimeAsUtilDateTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/TimestampAsSqlTimestampTest.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/TimestampAsUtilDateTest.java

clusterj_openjpa_DATA = $(clusterj_openjpa_version_JAR)

EXTRA_DIST = \
  $(clusterj_openjpa_res)/META-INF/services/* \
  $(clusterj_openjpa_res)/com/mysql/clusterj/openjpa/Bundle.properties \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/*.java \
  pom.xml

$(MANIFEST):
	echo "Manifest-Version: 1.0"  > $(MANIFEST)
	echo "Export-Package: com.mysql.clusterj.openjpa"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ API"  >> $(MANIFEST)
	echo "Bundle-Description: The API for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-openjpa" >> $(MANIFEST)

classclusterj_openjpa.stamp: $(clusterj_openjpa_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_openjpa.stamp

$(clusterj_openjpa_version_JAR) : classclusterj_openjpa.stamp $(MANIFEST)
	rm -f $@ ; $(JAR) cvfm $@ $(MANIFEST) \
	-C $(JAVAROOT) com/mysql/clusterj/openjpa \
	-C $(JAVAROOT) com/mysql/clusterj/openjpatest \
	-C $(clusterj_openjpa_res) com/mysql/clusterj/openjpa/Bundle.properties \
	-C $(clusterj_openjpa_res) META-INF \
	-C $(clusterj_openjpa_test_res) META-INF
	$(LN_CP_F) -f $(clusterj_openjpa_version_JAR) $(clusterj_openjpa_JAR)

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_openjpadir) && \
	$(LN_CP_F) -f $(clusterj_openjpa_version_JAR) $(clusterj_openjpa_JAR)

CLEANFILES= $(clusterj_openjpa_version_JAR) $(clusterj_openjpa_JAR) classclusterj_openjpa.stamp

clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/openjpa/*.class

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/target
