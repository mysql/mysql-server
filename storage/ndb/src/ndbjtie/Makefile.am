# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/java_support.mk.am

## ----------------------------------------------------------------------

## all subdirectories that have been configured (have a generated Makefile)
DIST_SUBDIRS = jtie mysql . test

## all subdirectories in DIST_SUBDIRS that should be built
SUBDIRS = jtie mysql . test

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = README.txt utils \
                $(MYSQL_DIR)/*.java \
                $(MGMAPI_DIR)/*.java \
                $(NDBAPI_DIR)/*.java \
		MysqlUtilsWrapper.hpp \
		NdbApiWrapper.hpp \
                mysql_utils_jtie.hpp \
                mgmapi_jtie.hpp \
                ndbapi_jtie.hpp \
                ndbjtie_defs.hpp \
                CMakeLists.txt

## ----------------------------------------------------------------------

## list of jar files to build
noinst_DATA = $(NDBJTIE_JAR)

## dependencies
JTIE_JAR = $(srcdir)/jtie/jtie-@JAVA_NDB_VERSION@.jar
CLASSPATH_ENV = CLASSPATH=.:$(JTIE_JAR)

## build directories
JAVAROOT=$(srcdir)/target/classes
JAVAHROOT=$(srcdir)/target

## jar file to be created
NDBJTIE_JAR = ndbjtie-@JAVA_NDB_VERSION@.jar

## java top-level class files
MYSQL_DIR = $(srcdir)/com/mysql/ndbjtie/mysql
MYSQL_java = \
		$(MYSQL_DIR)/Utils.java \
		$(MYSQL_DIR)/CharsetMap.java \
		$(MYSQL_DIR)/CharsetMapConst.java
MGMAPI_DIR = $(srcdir)/com/mysql/ndbjtie/mgmapi
MGMAPI_java = \
		$(MGMAPI_DIR)/MGMAPI.java
NDBAPI_DIR = $(srcdir)/com/mysql/ndbjtie/ndbapi
NDBAPI_java = \
		$(NDBAPI_DIR)/NDBAPI.java \
		$(NDBAPI_DIR)/NDBAPIConst.java \
		$(NDBAPI_DIR)/Ndb.java \
		$(NDBAPI_DIR)/NdbBlob.java \
		$(NDBAPI_DIR)/NdbBlobConst.java \
		$(NDBAPI_DIR)/NdbConst.java \
		$(NDBAPI_DIR)/NdbDictionary.java \
		$(NDBAPI_DIR)/NdbError.java \
		$(NDBAPI_DIR)/NdbErrorConst.java \
		$(NDBAPI_DIR)/NdbEventOperation.java \
		$(NDBAPI_DIR)/NdbEventOperationConst.java \
		$(NDBAPI_DIR)/NdbIndexOperation.java \
		$(NDBAPI_DIR)/NdbIndexOperationConst.java \
		$(NDBAPI_DIR)/NdbIndexScanOperation.java \
		$(NDBAPI_DIR)/NdbIndexScanOperationConst.java \
		$(NDBAPI_DIR)/NdbInterpretedCode.java \
		$(NDBAPI_DIR)/NdbLockHandle.java \
		$(NDBAPI_DIR)/NdbLockHandleConst.java \
		$(NDBAPI_DIR)/NdbOperation.java \
		$(NDBAPI_DIR)/NdbOperationConst.java \
		$(NDBAPI_DIR)/NdbRecAttr.java \
		$(NDBAPI_DIR)/NdbRecAttrConst.java \
		$(NDBAPI_DIR)/NdbRecord.java \
		$(NDBAPI_DIR)/NdbRecordConst.java \
		$(NDBAPI_DIR)/NdbScanFilter.java \
		$(NDBAPI_DIR)/NdbScanFilterConst.java \
		$(NDBAPI_DIR)/NdbScanOperation.java \
		$(NDBAPI_DIR)/NdbScanOperationConst.java \
		$(NDBAPI_DIR)/NdbTransaction.java \
		$(NDBAPI_DIR)/NdbTransactionConst.java \
		$(NDBAPI_DIR)/Ndb_cluster_connection.java \
		$(NDBAPI_DIR)/Ndb_cluster_connectionConst.java

classndbjtie.stamp: $(NDBAPI_java) $(MGMAPI_java) $(MYSQL_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	  $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classndbjtie.stamp

$(NDBJTIE_JAR): classndbjtie.stamp 
	rm -f $@; @JAR@ cvf $@  \
	  -C $(srcdir)/jtie/target/classes com/mysql/jtie \
	  -C $(JAVAROOT) com/mysql/ndbjtie

## generate JNI header files (for development only)
javah: $(NDBJTIE_JAR)
	$(JAVAH) -classpath $(NDBJTIE_JAR) \
		$(AM_JAVAHFLAGS) $(JAVAHFLAGS) \
		-d $(JAVAHROOT) \
		com.mysql.ndbjtie.mysql.Utils \
		com.mysql.ndbjtie.mysql.CharsetMap \
		com.mysql.ndbjtie.mgmapi.MGMAPI \
		com.mysql.ndbjtie.ndbapi.NDBAPI \
		com.mysql.ndbjtie.ndbapi.NDBAPIConst \
		com.mysql.ndbjtie.ndbapi.Ndb \
		com.mysql.ndbjtie.ndbapi.NdbBlob \
		com.mysql.ndbjtie.ndbapi.NdbDictionary \
		com.mysql.ndbjtie.ndbapi.NdbError \
		com.mysql.ndbjtie.ndbapi.NdbEventOperation \
		com.mysql.ndbjtie.ndbapi.NdbIndexOperation \
		com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation \
		com.mysql.ndbjtie.ndbapi.NdbInterpretedCode \
		com.mysql.ndbjtie.ndbapi.NdbOperation \
		com.mysql.ndbjtie.ndbapi.NdbRecAttr \
		com.mysql.ndbjtie.ndbapi.NdbRecord \
		com.mysql.ndbjtie.ndbapi.NdbScanFilter \
		com.mysql.ndbjtie.ndbapi.NdbScanOperation \
		com.mysql.ndbjtie.ndbapi.NdbTransaction \
		com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = \
		-I./utils \
		-I./jtie \
		-I./mysql \
		-I$(top_srcdir)/storage/ndb/include \
		-I$(top_srcdir)/storage/ndb/include/mgmapi \
		-I$(top_srcdir)/storage/ndb/include/ndbapi \
		@JNI_INCLUDE_DIRS@

## build a shared library
noinst_LTLIBRARIES = libndbjtie.la

## source files to build the library (added to distribuition by default)
libndbjtie_la_SOURCES = ndbjtie_lib.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#libndbjtie_la_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
#libndbjtie_la_LIBADD = \
#		../mysql/src/libmysqlutils.la \
#		$(top_srcdir)/storage/ndb/src/libndbclient.la
##   linking directly against a static (and not a libtool) library to ensure
##   self-containment violates libtool conventions and triggers a warning
##   about non-portability -- possibly, because of a potential pic/non-pic
##   code mix on some platforms.
#		../mysql/src/.libs/libmysqlutils.a \
#		$(top_srcdir)/storage/ndb/src/.libs/libndbclient.a

## mode-specific libtool options (overrides AM_LDFLAGS)
#libndbjtie_la_LDFLAGS = -no-undefined ## build self-contained library
#libndbjtie_la_LDFLAGS += -shared ## only create a shared library

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libndbjtie_la_LIBTOOLFLAGS =

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = 
CLEANFILES = classndbjtie.stamp $(NDBJTIE_JAR)

mostlyclean-local: 
	rm -rf $(top_srcdir)/storage/ndb/src/ndbjtie/target

## ----------------------------------------------------------------------
