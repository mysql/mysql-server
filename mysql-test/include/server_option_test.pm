#!/usr/bin/perl -w
# This module contains functions which will support the testing of mysqld options givven on command line
# or in the configuration file (my.cnf).
# Creator: Horst Hunger
# Date:    2012-09-17
#
package server_option_test;
use strict;

use base qw(Exporter);
our @EXPORT= qw(init_bootstrap fini_bootstrap check_bootstrap_log
                insert_option_my_cnf create_var_stmt my_cnf_bootstrap
	        commandline_bootstrap);

our $mysqld_bootstrap_cmd= "$ENV{'MYSQLD_BOOTSTRAP_CMD'} --basedir=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap --datadir=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/data/ --tmpdir=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/ --log-error=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/log/bootstrap.log --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB";

#
# Creates the directories needed by bootstrap cmd.
#
sub init_bootstrap {
mkdir("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap",0744);
mkdir("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/data",0744);
mkdir("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp",0744);
mkdir("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/log",0744);
}

#
# Removes the directories needed by bootstrap cmd.
#
sub fini_bootstrap {
use File::Path 'rmtree';
rmtree("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap");
}

#
# Creates a SELECT variable into file to be executed by bootstrap cmd.
# Bootstrap will fail if not such an input file will be given.
# The result of the sql execution has no meaning in the option test, but may also cause a bug
# and is therefore interesting for testing.
#
sub create_var_stmt ($) {
my $test_var= shift;
my @wrlines; 
# Input file for bootstrap cmd containing a SELECT var otherwise it is failing.
# It can also be used for test purposes.
my $fname= "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/bootstrap_in.sql";
if ($test_var) {
  $test_var=~ s/--//;
  $test_var=~ s/loose-//;
  $test_var=~ s/-/_/g;
  $test_var=~ s/=.*//;
  push(@wrlines,"SELECT \@\@GLOBAL.$test_var INTO OUTFILE \'$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/bootstrap.out\';\n");
  open(FILE, ">", $fname) or die "Error: Cannot open bootstrap_in.sql for writing.\n";
  print FILE @wrlines;
  close FILE;
}
}

#
# Checks the bootstrap.log if containing the pattern
#
sub check_bootstrap_log ($) {
my @patterns= @_;
my $found_pattern= 0;
my $fname= "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/log/bootstrap.log";
open(FILE, "<", $fname) or die "\nError: Cannot read bootstrap.log.\nError: Nothing has been written to the log file, which might be suspicious in case of wrong/missing value for the option. It is recommended to file a bug about the weak handling of such values.\n\n";
my @lines= <FILE>;
foreach my $one_line (@lines)
{
  foreach my $one_pattern (@_)
  {
   # print pattern, not line, to get a stable output
   # For test purposes: 
   # print "$one_line\n" if ($one_line =~ /$one_pattern/);
   $found_pattern= 1  if ($one_line =~ /$one_pattern/);
  }
}
close FILE;
return $found_pattern;
}

#
# Creates the my.cnf (for option in commandline).
#
sub create_my_cnf {
use strict;
my @wrlines; 
my $fname= "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/my.cnf";
@wrlines= (@wrlines, server_options_my_cnf(@wrlines));
@wrlines= (@wrlines, client_options_my_cnf(@wrlines));
open(FILE, ">", $fname) or die "Error: Cannot open my.cnf for writing.\n";
print FILE @wrlines;
close FILE;
}

#
# Inserts a startup option into my.cnf.
#
sub insert_option_my_cnf ($) {
use strict;
my $test_option= shift;
my @wrlines; 
my $fname= "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/my.cnf";
$test_option=~ s/--//;
@wrlines= (@wrlines, server_options_my_cnf(@wrlines));
push(@wrlines,$test_option."\n");
@wrlines= (@wrlines, client_options_my_cnf(@wrlines));
open(FILE, ">", $fname) or die "Error: Cannot open my.cnf for writing.\n";
print FILE @wrlines;
close FILE;
}

#
# Inserts server startup options into my.cnf.
#
sub server_options_my_cnf ($) {
use strict;
my @wrlines= shift; 
# Create a mini config file including the option to be tested.
# This config file is s the minimum to be given to the server.
push(@wrlines,"[mysqld]\n");
push(@wrlines,"lc-messages-dir=$ENV{'MYSQL_SHAREDIR'}\n");
push(@wrlines,"character-sets-dir=$ENV{'MYSQL_CHARSETSDIR'}\n");
push(@wrlines,"character-set-server=latin1\n");
push(@wrlines,"socket=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/mysqld.sock\n");
push(@wrlines,"server-id=1\n");
return (@wrlines);
}

#
# Inserts client startup options into my.cnf.
#
sub client_options_my_cnf ($) {
use strict;
my @wrlines= shift; 
push(@wrlines,"\n");
push(@wrlines,"[client]\n");
push(@wrlines,"password=\n");
push(@wrlines,"host=localhost\n");
push(@wrlines,"user=root\n");
push(@wrlines,"socket=$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/mysqld.sock\n");
return (@wrlines);
}

#
# Startup option on commandline and exec bootstrap with SELECT variable.
#
sub commandline_bootstrap ($$) {
my $test_option= shift;
my $additional_options= shift;
my $nb_rm_files= unlink("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/log/bootstrap.log");
$nb_rm_files= unlink("$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/bootstrap.out");
create_my_cnf ();
create_var_stmt ($test_option);
my $ret_code= qx($mysqld_bootstrap_cmd $test_option $additional_options< $ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap/tmp/bootstrap_in.sql 2>&1);
}

1;

