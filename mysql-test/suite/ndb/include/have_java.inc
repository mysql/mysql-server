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
# Parameters
#   $JAVA_MIN_REQUIRED_VERSION: Minimum required version of Java.
#
# If java executable is found, MTR_JAVA will be set to reflect this location
# If no java executable is found, fail the test in which this is included
my $java_args = "";
my @arch_paths = ();
my $java_min_req_version = $ENV{JAVA_MIN_REQUIRED_VERSION} ||
  die "ERROR: Java minimum required version not set, please set JAVA_MIN_REQUIRED_VERSION";

my $pathsep = "/";
my $bits = $ENV{MYSQL_SYSTEM_ARCHITECTURE};

# Architecture dependent paths

if(!IS_WINDOWS)
{
  push(@arch_paths, ('/usr/lib/jvm/java/bin', '/usr/lib64/jvm/java/bin'));
  push(@arch_paths, ('/usr/local/jdk/bin', '/usr/local/java/bin', '/usr/local/java/jdk/bin'));
  push(@arch_paths, ('/usr/jdk-latest/bin', '/usr/bin'));

  if ($bits == 64) {
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
my @java_unusable_paths = ();

sub java_exists {
   my ($path, $exe) = @_;

   $exe .= ".exe" if IS_WINDOWS;
   $path .= $pathsep if length($path) > 0;
   my $exists = $path . $exe;
   my $devnull = "/dev/null";
   $devnull = "NUL" if IS_WINDOWS;

   system("\"$exists\" " . $java_args . " -version > " . $devnull);
  
   my $ret = "";
   if($? == 0) {
     open(VER, "\"$exists\"  -version 2>&1 |");
     if (<VER> =~ /\"(.*)\"/) {
       my @java_version_arr = split /\./, $1;
       my @java_required_arr = split /\./, $java_min_req_version;
       my $i;
       for ($i = 0; $i < @java_required_arr; $i++) {
         if (($java_version_arr[$i] > $java_required_arr[$i]) ||
             ($i == (@java_required_arr - 1) &&
              $java_version_arr[$i] == $java_required_arr[$i])) {
           # Found Java with required version
           $ret = $exists;
           $java_version = $1;
           last;
         } elsif ($java_version_arr[$i] < $java_required_arr[$i]) {
           # Found Java has a version lower than minimum required
           push @java_unusable_paths, "$exists (version : $1)";
           last;
         }
       }
     }
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
  print "Java at following paths are unusable as a ".
        "minimum version of '$java_min_req_version' is required:\n"
     if @java_unusable_paths > 0;
  foreach my $unusable_path (@java_unusable_paths) {
    print " $unusable_path\n";
  }
  print $F "--die Could not find Java executable with minimum required version('$java_min_req_version');"
           ."please install Java in one of the above paths or set env MTR_JAVA to the preferred JAVA HOME;\n";
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
