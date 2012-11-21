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
DIST_SUBDIRS = . test

## all subdirectories in DIST_SUBDIRS that should be built
SUBDIRS = . test

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = $(JTIE_DIR)/*.java \
  CMakeLists.txt 

## ----------------------------------------------------------------------

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
noinst_DATA = $(JTIE_JAR)

## dependencies
CLASSPATH_ENV = CLASSPATH=.

JAVAROOT=$(srcdir)/target/classes

## jar file to be created
JTIE_JAR = jtie-@JAVA_NDB_VERSION@.jar

## java top-level class files
JTIE_DIR = $(srcdir)/com/mysql/jtie
JTIE_java = $(JTIE_DIR)/Wrapper.java \
	$(JTIE_DIR)/ArrayWrapper.java

classjtie.stamp: $(JTIE_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
          echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
          $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
            $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
        fi
	echo timestamp > classjtie.stamp

$(JTIE_JAR): classjtie.stamp $(MANIFEST)
	rm -f $@; @JAR@ cvf $@  \
          -C $(JAVAROOT) com/mysql/jtie

## generate JNI header files (for development only)
javah: $(MYJAPI_CLASSES)
	$(COMPILE.JAVAH) \
		com.mysql.jtie.Wrapper
		com.mysql.jtie.ArrayWrapper

## ----------------------------------------------------------------------

## cleanup lists
MOSTLYCLEANFILES = $(JTIE_DIR)/*.class 
CLEANFILES = classjtie.stamp $(JTIE_JAR)

mostlyclean-local: 
	rm -rf $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/target

## ----------------------------------------------------------------------
