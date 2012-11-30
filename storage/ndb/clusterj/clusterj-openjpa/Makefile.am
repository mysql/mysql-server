#   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

clusterj_openjpadir= $(prefix)/share/mysql/java/

clusterjpa_version_jar = clusterjpa-$(JAVA_NDB_VERSION).jar
clusterjpa_jar = clusterjpa.jar

OPENJPA_EXPORT=com.mysql.clusterj.openjpa

CLUSTERJPA_JAR_DIRS = clusterj-api clusterj-core clusterj-tie clusterj-openjpa 

CLUSTERJPA_MANIFEST = CLUSTERJPA_MANIFEST.MF

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/target/classes

CLASSPATH_ENV = CLASSPATH=@CLUSTERJ_CLASSPATH@:$(JAVAROOT):$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/target/classes:$(top_srcdir)/storage/ndb/clusterj/clusterj-jpatest/clusterj-jpatest-@JAVA_NDB_VERSION@.jar

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
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/NdbOpenJPAUtility.java 

clusterj_openjpa_tests = \
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

clusterj_openjpa_DATA = $(clusterjpa_version_jar)

EXTRA_DIST = \
  $(clusterj_openjpa_res)/META-INF/services/* \
  $(clusterj_openjpa_res)/com/mysql/clusterj/openjpa/Bundle.properties \
  $(clusterj_openjpa_src)/com/mysql/clusterj/openjpa/*.java \
  $(clusterj_openjpa_test)/com/mysql/clusterj/openjpatest/*.java \
  pom.xml CMakeLists.txt

$(CLUSTERJPA_MANIFEST):
	if test "$(OPENJPA_OPT)" == "clusterj-openjpa"; then \
	  echo "Manifest-Version: 1.0"  > $(CLUSTERJPA_MANIFEST); \
	  echo "Export-Package: $(CLUSTERJ_EXPORT),$(OPENJPA_EXPORT)"  >> $(CLUSTERJPA_MANIFEST); \
	  echo "Bundle-Name: ClusterJ API"  >> $(CLUSTERJPA_MANIFEST); \
	  echo "Bundle-Description: The API for ClusterJ"  >> $(CLUSTERJPA_MANIFEST); \
	fi;

classclusterj_openjpa.stamp: $(clusterj_openjpa_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_openjpa.stamp

# basically do nothing if OPENJPA_OPT is not set
$(clusterjpa_version_jar) : classclusterj_openjpa.stamp $(CLUSTERJPA_MANIFEST)
	rm -f $@; \
	@JAR@ cvfm $@ $(CLUSTERJPA_MANIFEST); \
	for i in $(CLUSTERJPA_JAR_DIRS); do \
	  BASE_NAME=`echo $$i | sed 's/clusterj-//'`; \
	  @JAR@ uvf $@ -C ../$$i/target/classes com/mysql; \
	  if test -x ../$$i/src/main/resources/META-INF/services; then \
	    @JAR@ uvf $@ -C ../$$i/src/main/resources META-INF/services; \
	  fi; \
	  BUNDLE=../$$i/src/main/resources/com/mysql/clusterj/$$BASE_NAME/Bundle.properties; \
	  if test -e $$BUNDLE; then \
	    @JAR@ uvf $@ -C ../$$i/src/main/resources com/mysql/clusterj/$$BASE_NAME/Bundle.properties; \
	  fi; \
	done; \
	@JAR@ uvf $@ -C ../../src/ndbjtie/target/classes com/mysql; \
	@JAR@ uvf $@ -C ../../src/ndbjtie/jtie/target/classes com/mysql; \
	echo $$CLASS_DIRS;

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_openjpadir) && \
	$(LN_CP_F) -f $(clusterjpa_version_jar) $(clusterjpa_jar)

CLEANFILES= classclusterj_openjpa.stamp

clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/openjpa/*.class

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-openjpa/target
