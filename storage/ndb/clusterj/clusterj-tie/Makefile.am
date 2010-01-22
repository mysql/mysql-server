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

clusterj_tie_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/src/main/java

clusterj_tie_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/src/main/resources

CLASSPATH_ENV = CLASSPATH=$(JAVAROOT):$(top_srcdir)/storage/ndb/ndbjtie/src/ndbjtie-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/ndbjtie/jtie/src/jtie-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/clusterj-core-@JAVA_NDB_VERSION@.jar

JAVAROOT = $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/target/classes

MANIFEST = MANIFEST.MF

clusterj_tiedir= $(prefix)/share/mysql/java/

clusterj_tie_version_JAR = clusterj-tie-@JAVA_NDB_VERSION@.jar
clusterj_tie_JAR = clusterj-tie.jar

clusterj_tie_java= \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/Utility.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/BlobImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterConnectionImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterConnectionServiceImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ClusterTransactionImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ColumnImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DbImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/DictionaryImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/IndexScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/OperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanFilterImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanOperationImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/ScanResultDataImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/TableImpl.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/KeyPart.java \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/PartitionKeyImpl.java

clusterj_tie_DATA = $(clusterj_tie_version_JAR) 

EXTRA_DIST = \
  src/main/resources/META-INF \
  src/main/resources/com/mysql/clusterj/tie/Bundle.properties \
  $(clusterj_tie_src)/com/mysql/clusterj/tie/*.java \
  pom.xml

$(MANIFEST):
	echo "Manifest-Version: 1.0"  > $(MANIFEST)
	echo "Export-Package: com.mysql.clusterj.tie"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ API"  >> $(MANIFEST)
	echo "Bundle-Description: The API for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-tie" >> $(MANIFEST)

classclusterj_tie.stamp: $(clusterj_tie_java)
	mkdir -p  $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classclusterj_tie.stamp

$(clusterj_tie_version_JAR) : classclusterj_tie.stamp $(MANIFEST)
	rm -f $@ ; @JAR@ cvfm $@ $(MANIFEST) -C src/main/resources META-INF \
	  -C src/main resources/com/mysql/clusterj/tie/Bundle.properties \
	  -C $(JAVAROOT) com/mysql/clusterj
	$(LN_CP_F) -f $(clusterj_tie_version_JAR) $(clusterj_tie_JAR)

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_tiedir) && \
	$(LN_CP_F) -f $(clusterj_tie_version_JAR) $(clusterj_tie_JAR)


CLEANFILES= $(clusterj_tie_version_JAR) $(clusterj_tie_JAR) classclusterj_tie.stamp

clean-local:
	rm -rf $(JAVAROOT)/com/mysql/clusterj/tie/*.class

MOSTLYCLEANFILES = $(MANIFEST)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-tie/target
