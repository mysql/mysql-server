--perl
use strict;
use File::Basename;
use IO::File;

#Check if script is installed

my $script = `which script`;
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_script.result", "w") or die;
if(!$script)
{
  print $F "skip Script is not installed;\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_script.result
