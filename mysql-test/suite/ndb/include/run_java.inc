##
## run_java.inc - include script to run Java
##
## Parameters
##   $MTR_JAVA: Java which should be used
##   $JAVA_CLASS: Class file to run
##   $JAVA_CLASSPATH: Class path to use when running test
##   $JAVA_JVM_OPTS: Optional parameters to the JVM
##
##
## Usage:
##
## --source suite/ndb/include/have_java.inc
##
## let $JAVA_CLASSPATH=-classpath .:/some/path;
## let $JAVA_JVM_OPTS=-Dsome.setting=123 -ea -Xcheck:jni;
##
## let $JAVA_CLASS=test/SomeTest.class;
## --source suite/ndb/include/run_java.inc
##
## let $JAVA_CLASS=test/SomeOtherTest.class;
## --source suite/ndb/include/run_java.inc
##

--let $COMMAND= $MTR_JAVA $MTR_JAVA_ARGS
--let $COMMAND= $COMMAND $JAVA_JVM_OPTS
--let $COMMAND= $COMMAND $JAVA_CLASSPATH
--let $COMMAND= $COMMAND $JAVA_CLASS $JAVA_ARGUMENTS

--exec $COMMAND 2>&1
