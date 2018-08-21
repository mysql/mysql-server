--perl
use strict;
use File::Basename;
use IO::File;
use lib "lib/";
use My::Platform;
use My::Find;

require "lib/mtr_misc.pl";

#
# Looks for java executable (/bin/java) in several locations:
# - MTR_JAVA environment variable
# - Other architecture-dependent locations (see below)
# - $PATH environment variable
#
# If java executable is found, MTR_JAVA will be set to reflect this location
# If no java executable is found, fail the test in which this is included
my $java_args = "";
my @arch_paths = ();

my $pathsep = "/";
my $bits = $ENV{MYSQL_SYSTEM_ARCHITECTURE};

# Architecture dependent paths

if(!IS_WINDOWS)
{
  push(@arch_paths, ('/usr/lib/jvm/java/bin', '/usr/lib64/jvm/java/bin'));
  push(@arch_paths, ('/usr/local/jdk/bin', '/usr/local/java/bin', '/usr/local/java/jdk/bin'));
  push(@arch_paths, ('/usr/jdk-latest/bin', '/usr/bin'));

  $java_args = "-d32";
  if ($bits == 64) {
    $java_args = "-d64";
    push(@arch_paths, ('/usr/java-local/jdk-64/bin', '/usr/local/jdk-64/bin', '/usr/local/java/jdk-64/bin'));
  }
}
else
# IS_WINDOWS
{
  if ($bits == 64) {
    push(@arch_paths, 'C:\java\jdk-64\bin');
    $java_args = "-server";
  } else {
    push(@arch_paths, 'C:\java\jdk\bin');
    $java_args = "-client"; 
  }
  push(@arch_paths, 'C:\Windows\SysWOW64');
  push(@arch_paths, 'C:\Windows\System32');
}

my @java_paths = ($ENV{JAVA_HOME} . $pathsep . "bin");
push(@java_paths, $ENV{JDK_HOME} . $pathsep . "bin");
push(@java_paths, @arch_paths);
push(@java_paths, "*"); # last resort: empty path marker for "java on PATH variable"

my $java_loc = "";
our $java_version = "";

sub java_exists {
   my ($path, $exe) = @_;

   $exe .= ".exe" if IS_WINDOWS;
   $path .= $pathsep if length($path) > 0;
   my $exists = $path . $exe;
   my $devnull = "/dev/null";
   $devnull = "NUL" if IS_WINDOWS;

   system("\"$exists\" " . $java_args . " -version > " . $devnull);
  
   my $ret =  $exists if ($? == 0);
   if($? == 0) {
     my $ret = $exists;
     open(VER, "\"$exists\"  -version 2>&1 |");
     $java_version = <VER>;
     close(VER);
   }
   
   return $ret;
}

system("echo Looking for a suitable Java...");
foreach my $path (@java_paths) 
{
  if (!defined $path || $path eq ($pathsep . "bin")) {
    # no MTR_JAVA case
    next;
  }
  if ($path eq "*" ) {
   # java on PATH env case
   $path = "";
  }

  my $tmp = java_exists($path, "java");

  if ($tmp ne "") 
  {
     $java_loc = $tmp;
     last;
   }
}

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_java_result.inc", "w") or die;
if ($java_loc eq '') {
  print $F "--die Could not find Java executable; please install Java in one of the above paths or set env MTR_JAVA to the preferred JAVA HOME;\n";
} else {
  print $F "--let \MTR_JAVA= $java_loc\n";
  print $F "--let \MTR_JAVA_ARGS= $java_args\n";
  print $F "--let \MTR_JAVA_VERSION= $java_version\n";
  print $F "--echo Found Java\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_java_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/have_java_result.inc
