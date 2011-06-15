#   Copyright 2010 Sun Microsystems, Inc.
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

CLUSTERJ_EXPORT=com.mysql.clusterj,com.mysql.clusterj.annotation,com.mysql.clusterj.query,com.mysql.clusterj.core,com.mysql.clusterj.core.metadata,com.mysql.clusterj.core.query,com.mysql.clusterj.core.spi,com.mysql.clusterj.core.store,com.mysql.clusterj.core.util,com.mysql.clusterj.jdbc,com.mysql.clusterj.jdbc.antlr,com.mysql.clusterj.jdbc.antlr.node,com.mysql.clusterj.tie

BASE_DIRS = clusterj-api clusterj-core clusterj-tie

SUBDIRS = $(BASE_DIRS) $(CLUSTERJ_TESTS) $(OPENJPA_OPT) $(CLUSTERJ_JDBC_OPT)

DIST_SUBDIRS = clusterj-api clusterj-core clusterj-tie clusterj-jpatest clusterj-openjpa clusterj-test clusterj-jdbc

NDBJTIE_DIRS = $(top_srcdir)/storage/ndb/src/ndbjtie $(top_srcdir)/storage/ndb/src/ndbjtie/jtie

CLUSTERJ_MANIFEST = CLUSTERJ_MANIFEST.MF

clusterjdir= $(prefix)/share/mysql/java/

clusterj_version_jar = clusterj-$(JAVA_NDB_VERSION).jar
clusterj_jar = clusterj.jar

clusterj_DATA = $(clusterj_version_jar)

EXTRA_DIST = pom.xml CMakeLists.txt

class_timestamps = clusterj-api/classclusterj_api.stamp \
                   clusterj-core/classclusterj_core.stamp \
                   clusterj-tie/classclusterj_tie.stamp

$(CLUSTERJ_MANIFEST):
	echo "Manifest-Version: 1.0"  > $(CLUSTERJ_MANIFEST)
	echo "Export-Package: $(CLUSTERJ_EXPORT)"  >> $(CLUSTERJ_MANIFEST) 
	echo "Bundle-Name: ClusterJ"  >> $(CLUSTERJ_MANIFEST)
	echo "Bundle-Description: ClusterJ"  >> $(CLUSTERJ_MANIFEST)

# make both clusterj and clusterjpa jar (the latter optional based on OPENJPA_OPT)
$(clusterj_version_jar) : $(BASE_DIRS) $(CLUSTERJ_TESTS) $(OPENJPA_OPT) $(CLUSTERJ_MANIFEST) $(class_timestamps)
	rm -f $(clusterj_version_jar); \
	@JAR@ cvfm $(clusterj_version_jar) $(CLUSTERJ_MANIFEST); \
	for i in $(BASE_DIRS); do \
	  BASE_NAME=`echo $$i | sed 's/clusterj-//'`; \
	  @JAR@ uvf $@ -C $$i/target/classes com/mysql/clusterj; \
	  if test -x $$i/src/main/resources/META-INF/services; then \
	    @JAR@ uvf $@ -C $$i/src/main/resources META-INF/services; \
	  fi; \
	 BUNDLE=$$i/src/main/resources/com/mysql/clusterj/$$BASE_NAME/Bundle.properties; \
	  if test -e $$BUNDLE; then \
	    @JAR@ uvf $@ -C $$i/src/main/resources com/mysql/clusterj/$$BASE_NAME/Bundle.properties; \
	  fi; \
	done; \
	@JAR@ uvf $@ -C ../src/ndbjtie/target/classes com/mysql/ndbjtie; \
	@JAR@ uvf $@ -C ../src/ndbjtie/jtie/target/classes com/mysql/jtie; \
	echo $$CLASS_DIRS

install-data-hook:
	cd $(DESTDIR)/$(clusterjdir) && \
	$(LN_CP_F) -f $(clusterj_version_jar) $(clusterj_jar) 

CLEANFILES= $(clusterj_version_jar) $(clusterj_jar) $(clusterjpa_version_jar) $(clusterjpa_jar)

MOSTLYCLEANFILES = $(CLUSTERJPA_MANIFEST) $(CLUSTERJ_MANIFEST)
