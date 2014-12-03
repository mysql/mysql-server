--perl
use strict; 
use lib "lib/";
use My::Find;
use File::Basename;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

my $NDB_INDEX_STAT =  my_find_bin($bindir,
                           ["storage/ndb/tools", "bin"],
                           "ndb_index_stat");

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR" ;
open ( OUTPUT, ">$vardir/tmp/temp.inc") ;
print OUTPUT "let \$NDB_INDEX_STAT=$NDB_INDEX_STAT;\n";
close (OUTPUT);
EOF
