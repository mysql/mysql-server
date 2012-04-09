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
# - JAVA_HOME/bin/java
# - Other hard-coded locations (see below)
# - $PATH
#
# If java executable is found, MTR_JAVA will be set to reflect this location
#
my $java_args = "";
my @arch_paths = ();

sub getRegValue
{
  my ($key, $valuename) = @_;
  my $cmd = "reg query \"$key\" /v $valuename";

  my $res = "";

  my @ares = `$cmd`;
  if($? == 0)
  {
    foreach my $row (@ares)
    {
      if($row =~ m/.*$valuename\s*REG_SZ\s*(.*)/)
      {
        $row = $1;
        $row =~ s/^\s*//;
        $row =~ s/\s*$//;
        $res = $row;
        last;
      }
    }
  }
  return $res;
}

sub getJavaPath
{
  my $key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit";
  my $version = getRegValue($key, "CurrentVersion");

  if(length($version) < 1)
  {
    return "";
  }

  $key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit\\$version";
  my $path = getRegValue($key, "JavaHome");

  return $path;
}

my $pathsep = "/";

if(!IS_WINDOWS)
{
  push(@arch_paths, ('/usr/lib/jvm/java', '/usr/lib64/jvm/java'));

  $java_args = "-d32";

  # Architecture dependent paths, will be expanded with -64 if we are running on 64 bit build
  my $bits = $ENV{MYSQL_SYSTEM_ARCHITECTURE};
  if ($bits == 64) {
    map { $_ . "-64" } @arch_paths;
    $java_args = "-d64";
    push(@arch_paths, ('/usr/local/jdk', '/usr/local/java', '/usr/local/java/jdk'));
    push(@arch_paths, ('/usr/java-local/jdk-64/', '/usr/local/jdk-64', '/usr/local/java/jdk-64'));
  } else {
    push(@arch_paths, ('/usr/local/jdk', '/usr/local/java', '/usr/local/java/jdk'));
  }
}
else
{ 
  my $win_path = getJavaPath();
  if(length($win_path) > 0) 
  {
    push(@arch_paths, $win_path);
  }
  $pathsep = "\\";
}


my @java_paths = ($ENV{MTR_JAVA}, $ENV{JAVA_HOME}, $ENV{JDK_HOME});

push(@java_paths, @arch_paths);
push(@java_paths, "*"); # empty path for "java on PATH variable"

my $java_loc = "";
our $java_version = "";

sub java_exists {
   my ($path, $exe) = @_;

   $exe .= ".exe" if IS_WINDOWS;
   $path .= $pathsep if length($path) > 0;
   my $exists = $path . $exe;
   my $devnull = "/dev/null";
   $devnull = "NUL" if IS_WINDOWS;

   system($exists . " " . $java_args . " -version > " . $devnull);
  
   my $ret =  $exists if ($? == 0);
   if($? == 0) {
     my $ret = $exists;
     open(VER, "$exists  -version 2>&1 |");
     $java_version = <VER>;
     close(VER);
   }
   
   return $ret;
}

foreach my $path (@java_paths) 
{
  if (!defined $path || $path eq '') {
    next;
  }
  if ($path eq "*" ) {
    # java on PATH env case
    $path = "";
  } else {
    # std case
    $path .= $pathsep . "bin";
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
  print $F "--skip Could not find Java executable;\n";
} else {
  print $F "--let \MTR_JAVA= $java_loc\n";
  print $F "--let \MTR_JAVA_ARGS= $java_args\n";
  print $F "--let \MTR_JAVA_VERSION= $java_version\n";
  print $F "--echo Found Java: '\$MTR_JAVA'\n";
  print $F "--echo Java version: '\$MTR_JAVA_VERSION'\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_java_result.inc
