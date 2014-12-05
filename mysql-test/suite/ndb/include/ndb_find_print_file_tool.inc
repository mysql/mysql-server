--perl
use strict; 
use lib "lib/";
use My::Find;
use File::Basename;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

my $NDB_PRINT_FILE =  my_find_bin($bindir,
                           ["storage/ndb/src/kernel/blocks", "bin"],
                           "ndb_print_file");

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR" ;
open ( OUTPUT, ">$vardir/tmp/ndb_find_print_file_tool_result.inc") ;
print OUTPUT "let \$NDB_PRINT_FILE=$NDB_PRINT_FILE;\n";
close (OUTPUT);
EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_find_print_file_tool_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/ndb_find_print_file_tool_result.inc
