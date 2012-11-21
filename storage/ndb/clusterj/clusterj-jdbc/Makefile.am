#   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
#
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

clusterj_jdbc_src = $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/java

clusterj_jdbc_res = $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/resources

MANIFEST = MANIFEST.MF

JAVAROOT=$(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target/classes

CLASSPATH_ENV = CLASSPATH=@CLUSTERJ_CLASSPATH@:$(JAVAROOT):$(top_srcdir)/storage/ndb/clusterj/clusterj-api/clusterj-api-@JAVA_NDB_VERSION@.jar:$(top_srcdir)/storage/ndb/clusterj/clusterj-core/target/classes

clusterj_jdbcdir= $(prefix)/share/mysql/java/

clusterj_jdbc_version_JAR = clusterj-jdbc-@JAVA_NDB_VERSION@.jar
clusterj_jdbc_JAR = clusterj-jdbc.jar

clusterj_jdbc_java = \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/AbstractResultSetInternalMethods.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ConnectionLifecycleInterceptor.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/DomainFieldHandlerImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/DomainTypeHandlerImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/InterceptorImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/QueryExecutionContextJDBCImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ResultSetInternalMethodsImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ResultSetInternalMethodsUpdateCount.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/SQLExecutor.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/StatementInterceptor.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ValueHandlerBatchingJDBCSetImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ValueHandlerBindValuesImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/ValueHandlerImpl.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/ANTLRNoCaseFileStream.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/ANTLRNoCaseStringStream.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/BaseErrorListener.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/ErrorListener.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/MySQLLexer.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/MySQLParser.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/QueuingErrorListener.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/RecognizerErrorDelegate.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/AndNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/BetweenNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/BinaryOperatorNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/BooleanOperatorNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/CommandNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/DeleteNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/EqualsNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/GreaterEqualsNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/GreaterThanNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/InsertNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/LessEqualsNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/LessThanNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/Node.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/NotNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/OrNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/ParensNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/PlaceholderNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/PredicateNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/SelectNode.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/WhereNode.java 

clusterj_jdbc_DATA = $(clusterj_jdbc_version_JAR)

$(MANIFEST): 
	echo "Manifest-Version: 1.0"  > $(MANIFEST) 
	echo "Export-Package: com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation"  >> $(MANIFEST)
	echo "Bundle-Name: ClusterJ JDBC"  >> $(MANIFEST)
	echo "Bundle-Description: The JDBC interception for ClusterJ"  >> $(MANIFEST)
	echo "Bundle-SymbolicName: com.mysql.clusterj.clusterj-jdbc" >> $(MANIFEST)

clusterj_jdbc_antlr = \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/antlr3/com/mysql/clusterj/jdbc/antlr/MySQL51Parser.g \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/antlr3/com/mysql/clusterj/jdbc/antlr/MySQL51Lexer.g \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/antlr3/imports/MySQL51Functions.g

clusterj_jdbc_antlr_src = \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target/generated-sources/antlr3/com/mysql/clusterj/jdbc/antlr/MySQL51Parser.java \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target/generated-sources/antlr3/com/mysql/clusterj/jdbc/antlr/MySQL51Lexer.java \
  $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target/generated-sources/antlr3/com/mysql/clusterj/jdbc/antlr/MySQL51Parser_MySQL51Functions.java

classclusterj_jdbc.stamp: $(clusterj_jdbc_java) $(clusterj_jdbc_antlr)
	if ! test -e classclusterj_jdbc_antlr.stamp; then \
	  mkdir -p $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target/generated-sources/antlr3; \
	  cd $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/src/main/antlr3 && \
	    $(CLASSPATH_ENV) java org.antlr.Tool \
	    -lib imports \
	    -o  @abs_top_srcdir@/storage/ndb/clusterj/clusterj-jdbc/target/generated-sources/antlr3 \
	    com/mysql/clusterj/jdbc/antlr/MySQL51Lexer.g \
	    com/mysql/clusterj/jdbc/antlr/MySQL51Parser.g; \
	    cd @abs_top_srcdir@/storage/ndb/clusterj/clusterj-jdbc; \
	  echo timestamp > classclusterj_jdbc_antlr.stamp; \
	fi
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $(clusterj_jdbc_java) $(clusterj_jdbc_antlr_src)' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	    $(AM_JAVACFLAGS) $(JAVACFLAGS) $(clusterj_jdbc_java) $(clusterj_jdbc_antlr_src); \
	fi
	echo timestamp > classclusterj_jdbc.stamp

$(clusterj_jdbc_version_JAR): classclusterj_jdbc.stamp $(MANIFEST)
	rm -f $@; @JAR@ cvfm $@ $(MANIFEST) \
	  -C $(JAVAROOT) com/mysql/clusterj 

## create a versioned link
install-data-hook:
	cd $(DESTDIR)/$(clusterj_jdbcdir) && \
	$(LN_CP_F) -f $(clusterj_jdbc_version_JAR) $(clusterj_jdbc_JAR)

CLEANFILES= $(clusterj_jdbc_version_JAR) classclusterj_jdbc_antlr.stamp classclusterj_jdbc.stamp

clean-local:
	rm -f $(JAVAROOT)/com/mysql/clusterj/*.class \
  $(JAVAROOT)/com/mysql/clusterj/query/*.class \
  $(JAVAROOT)/com/mysql/clusterj/annotation/*.class \
  $(clusterj_jdbc_antlr_src)

MOSTLYCLEANFILES = $(MANIFEST)

EXTRA_DIST = \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/*.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/*.java \
  $(clusterj_jdbc_src)/com/mysql/clusterj/jdbc/antlr/node/*.java \
  logging.properties pom.xml CMakeLists.txt \
  $(clusterj_jdbc_antlr)

mostlyclean-local: clean-local
	rm -rf $(top_srcdir)/storage/ndb/clusterj/clusterj-jdbc/target

