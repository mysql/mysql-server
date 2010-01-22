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

if COND_OPENJPA_OPT 
  OPENJPA_OPT=clusterj-openjpa 
  OPENJPA_EXPORT=,com.mysql.clusterj.openjpa
endif

if COND_CLUSTERJ_TESTS
  CLUSTERJ_OPT_TEST=clusterj-test
endif
if COND_OPENJPA_TESTS
  OPENJPA_OPT_TEST=clusterj-jpatest
endif


CLUSTERJ_EXPORT=com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation, com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie

BASE_DIRS = clusterj-api clusterj-core clusterj-tie $(OPENJPA_OPT)

SUBDIRS = $(BASE_DIRS) $(OPENJPA_OPT_TEST) $(CLUSTERJ_OPT_TEST) $(OPENJPA_OPT_TEST)

DIST_SUBDIRS = clusterj-api clusterj-core clusterj-tie clusterj-openjpa clusterj-test clusterj-jpatest

MANIFEST = MANIFEST.MF

clusterjdir= $(prefix)/share/mysql/java/

clusterj_version_JAR = mysql-cluster-connector-java-$(JAVA_NDB_VERSION).jar
clusterj_JAR = mysql-cluster-connector-java.jar

clusterj_DATA = $(clusterj_version_JAR) 

EXTRA_DIST = pom.xml

$(MANIFEST):
	echo "Manifest-Version: 1.0"  > $(MANIFEST)
	echo "Export-Package: $(CLUSTERJ_EXPORT)$(OPENJPA_EXPORT)"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ API"  >> $(MANIFEST)
	echo "Bundle-Description: The API for ClusterJ"  >> $(MANIFEST)

$(clusterj_version_JAR) : $(SUBDIRS) $(MANIFEST)
	rm -f $@; CLASS_DIRS=; for i in $(BASE_DIRS); do \
	  BASE_NAME=`echo $$i | sed 's/clusterj-//'`; \
	  CLASS_DIRS="$$CLASS_DIRS -C $$i/target/classes com/mysql/clusterj"; \
	  if test -x $$i/src/main/resources/META-INF/services; then \
	    CLASS_DIRS="$$CLASS_DIRS -C $$i/src/main/resources META-INF/services"; \
	  fi; \
	  BUNDLE=$$i/src/main/resources/com/mysql/clusterj/$$BASE_NAME/Bundle.properties; \
	  if test -e $$BUNDLE; then \
	    CLASS_DIRS="$$CLASS_DIRS -C $$i/src/main/resources com/mysql/clusterj/$$BASE_NAME/Bundle.properties"; \
	  fi; \
	done; \
	echo $$CLASS_DIRS; \
	@JAR@ cvfm $@ $(MANIFEST); @JAR@ uvf $@ $$CLASS_DIRS
	$(LN_CP_F) -f $(clusterj_version_JAR) $(clusterj_JAR)

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterjdir) && \
	$(LN_CP_F) -f $(clusterj_version_JAR) $(clusterj_JAR)

CLEANFILES= $(clusterj_version_JAR) $(clusterj_JAR)

MOSTLYCLEANFILES = $(MANIFEST)
