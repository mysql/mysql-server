#   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include $(top_srcdir)/storage/ndb/config/java_support.mk.am

clusterj_api_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-api/src/main/java

clusterj_api_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-api/src/main/resources

MANIFEST = MANIFEST.MF

JAVAROOT=$(top_srcdir)/storage/ndb/clusterj/clusterj-api/target/classes

CLASSPATH_ENV = CLASSPATH=$(JAVAROOT)

clusterj_apidir= $(prefix)/share/mysql/java/

clusterj_api_version_JAR = clusterj-api-@JAVA_NDB_VERSION@.jar
clusterj_api_JAR = clusterj-api.jar

clusterj_api_java = \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Column.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Columns.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Extension.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Extensions.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Index.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Indices.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Lob.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/NotPersistent.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/NullValue.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/PartitionKey.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/PersistenceCapable.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/PersistenceModifier.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/Persistent.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/PrimaryKey.java \
  $(clusterj_api_src)/com/mysql/clusterj/Results.java \
  $(clusterj_api_src)/com/mysql/clusterj/Query.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/Predicate.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/PredicateOperand.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/QueryDefinition.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/QueryDomainType.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/QueryBuilder.java \
  $(clusterj_api_src)/com/mysql/clusterj/Transaction.java \
  $(clusterj_api_src)/com/mysql/clusterj/Session.java \
  $(clusterj_api_src)/com/mysql/clusterj/SessionFactory.java \
  $(clusterj_api_src)/com/mysql/clusterj/SessionFactoryService.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJException.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJDatastoreException.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJFatalException.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJFatalInternalException.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJFatalUserException.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJHelper.java \
  $(clusterj_api_src)/com/mysql/clusterj/ClusterJUserException.java \
  $(clusterj_api_src)/com/mysql/clusterj/Constants.java \
  $(clusterj_api_src)/com/mysql/clusterj/LockMode.java \
  $(clusterj_api_src)/com/mysql/clusterj/ColumnMetadata.java \
  $(clusterj_api_src)/com/mysql/clusterj/ColumnType.java \
  $(clusterj_api_src)/com/mysql/clusterj/Dbug.java \
  $(clusterj_api_src)/com/mysql/clusterj/DynamicObject.java \
  $(clusterj_api_src)/com/mysql/clusterj/DynamicObjectDelegate.java


clusterj_api_DATA = $(clusterj_api_version_JAR)

EXTRA_DIST = \
  $(clusterj_api_src)/com/mysql/clusterj/*.java \
  $(clusterj_api_src)/com/mysql/clusterj/package.html \
  $(clusterj_api_src)/com/mysql/clusterj/query/*.java \
  $(clusterj_api_src)/com/mysql/clusterj/query/package.html \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/*.java \
  $(clusterj_api_src)/com/mysql/clusterj/annotation/package.html \
  pom.xml CMakeLists.txt

$(MANIFEST): 
	echo "Manifest-Version: 1.0"  > $(MANIFEST) 
	echo "Export-Package: com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ API"  >> $(MANIFEST)
	echo "Bundle-Description: The API for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-api" >> $(MANIFEST)

classclusterj_api.stamp: $(clusterj_api_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_api.stamp

$(clusterj_api_version_JAR): classclusterj_api.stamp $(MANIFEST)
	rm -f $@; @JAR@ cvfm $@ $(MANIFEST) \
	  -C $(JAVAROOT) com/mysql/clusterj 

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_apidir) && \
	$(LN_CP_F) -f $(clusterj_api_version_JAR) $(clusterj_api_JAR)

CLEANFILES= $(clusterj_api_version_JAR) classclusterj_api.stamp

clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/*.class \
  $(JAVAROOT)/com/mysql/clusterj/query/*.class \
  $(JAVAROOT)/com/mysql/clusterj/annotation/*.class 

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-api/target

