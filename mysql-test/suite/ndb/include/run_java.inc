--perl
use strict;
use lib "lib/";
use My::Exec;
use My::Platform;

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
## let JAVA_CLASSPATH=-classpath .:/some/path;
## let JAVA_JVM_OPTS=-Dsome.setting=123 -ea -Xcheck:jni;
##
## let JAVA_CLASS=test/SomeTest.class;
## --source suite/ndb/include/run_java.inc
##
## let JAVA_CLASS=test/SomeOtherTest.class;
## --source suite/ndb/include/run_java.inc
##

# Check parameters
my $java = $ENV{MTR_JAVA} || die "ERROR: Java not found, set MTR_JAVA";
my $java_classpath = $ENV{JAVA_CLASSPATH} || die "ERROR: Java classpath not set, please set JAVA_CLASSPATH";
my $java_class = $ENV{JAVA_CLASS} || die "ERROR: Java class to run not set, please set JAVA_CLASS";
my $java_args = $ENV{MTR_JAVA_ARGS} || '';
my $jvm_opts = $ENV{JAVA_JVM_OPTS} || '';
my $class_args = $ENV{JAVA_ARGUMENTS} || '';
my $vardir = $ENV{MYSQLTEST_VARDIR} || die "Need MYSQLTEST_VARDIR";

my $sep = ":";
if(IS_WINDOWS)
{
  $sep = ";";
}


# The length of the variables that can be passed in environment variables are limited
# (around 1024 characters) - this trick lets you plit the classpath in several variables to
# be avle to pass longer classpaths
for my $i (1..9)
{
  my $env = $ENV{"JAVA_CLASSPATH_$i"} || '';
  if ($env)
  {
    $java_classpath .= "$sep$env";
  }
  my $jvm = $ENV{"JAVA_JVM_OPTS_$i"} || '';
  if ($jvm)
  {
    $jvm_opts .= " $jvm";
  }
}

# Tell the vm to put temporary files in $MYSQLTEST_VARDIR/tmp
$jvm_opts .= " -Djava.io.tmpdir=$vardir/tmp";

if ($ENV{MTR_CLASSPATH})
{
  $java_classpath .= "$sep$ENV{MTR_CLASSPATH}"
}

my $cmd = "\"$java\" $java_args $jvm_opts -classpath \"$java_classpath\" $java_class $class_args";
my $res = exec_print_on_error($cmd);

my $F = IO::File->new("$vardir/tmp/run_java.result", "w") || die "Couldn't open varfile for writing";
if ($res)
{
  print $F "# Success\n";
}
else
{
  # Rather than --die, we change the output
  # That will cause a result-content-mismatch giving more
  # visible context info
  print $F "--echo \'$cmd\' run failed;\n";
}
$F->close();

EOF
--source $MYSQLTEST_VARDIR/tmp/run_java.result
--remove_file $MYSQLTEST_VARDIR/tmp/run_java.result
