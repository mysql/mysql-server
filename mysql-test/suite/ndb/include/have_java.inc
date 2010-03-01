--perl
use strict;
use IO::File;
use lib "lib/";
use My::Platform;

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
my $arch_paths = ['/usr/local/jdk', '/usr/local/java', '/usr/local/java/jdk'];

if(!IS_WINDOWS)
{
  $java_args = "-d32";

  # Architecture dependent paths, will be expanded with -64 if we are running on 64 bit build
  my $bits = $ENV{MYSQL_SYSTEM_ARCHITECTURE};
  if ($bits == 64) {
    $arch_paths = [map { $_ . "-64" } @$arch_paths];
    $java_args = "-d64";
  }
}

my $java_paths = [$ENV{MTR_JAVA}, $ENV{JAVA_HOME}, $ENV{JDK_HOME}, '/usr/lib/jvm/java',
                  '/usr/lib64/jvm/java', @$arch_paths,
                  '/System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK'];
my $java_loc = "";

sub java_exists {
   my $path = shift;

   system($path . " -version > /dev/null");
   my $exists = ($? == 0); 

   # Debug:
   # print STDERR "Check if java exists: path: '$path', exists: '$exists'\n";

   return $exists;
}

foreach my $path (@$java_paths) {
  if (defined $path && $path ne '') {
     if (java_exists($path . "/bin/java")) {
        $java_loc = $path . "/bin/java";
        last;
     }
  }
}

# If everything else fails, check if there exists a java in out PATH
if ($java_loc eq '') {
   if (java_exists('java')) {
      $java_loc = 'java';
   }
}

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_java_result.inc", "w") or die;
if ($java_loc eq '') {
  print $F "--skip Could not find Java executable;\n";
} else {
  print $F "--let \MTR_JAVA= $java_loc\n";
  print $F "--let \MTR_JAVA_ARGS= $java_args\n";
  print $F "--echo Found Java: '\$MTR_JAVA'\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_java_result.inc
