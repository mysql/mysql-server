--perl
use strict; 
use lib "lib/";
use My::Find;
use File::Basename;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);

# In the RPM case, binaries and libraries are installed in the
# default system locations, instead of having our own private base
# directory. And we install "usr/share/mysql-test". Moving up two
# more directory relative to "mysql-test" gives us a usable base
# directory for RPM installs.
if (! -d "$basedir/sql" and ! -d "$basedir/bin" )
{
  $basedir= dirname($basedir);
}
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

my $NDB_PERROR =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_perror");

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR" ;
open ( OUTPUT, ">$vardir/tmp/ndb_find_ndb_perror.inc") ;
print OUTPUT "let \$NDB_PERROR=$NDB_PERROR;\n";
close (OUTPUT);
EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_find_ndb_perror.inc
--remove_file $MYSQLTEST_VARDIR/tmp/ndb_find_ndb_perror.inc
