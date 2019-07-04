# This inc-file should be called in all test case that does
# ndb_backup and ndb_restore. This inc-file define the path
# for backup directory and also include the script to find the
# path of "ndb_restore" binary in test suite.

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

my $NDB_RESTORE =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_restore");

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR" ;
open ( OUTPUT, ">$vardir/tmp/ndb_find_restore_result.inc") ;
print OUTPUT "let \$NDB_RESTORE=$NDB_RESTORE;\n";
close (OUTPUT);
EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_find_restore_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/ndb_find_restore_result.inc

# BACKUP_RESTORE SET-UP

--disable_query_log
if (!$NDB_BACKUP_DIR)
{
  --die Test requires NDB_BACKUP_DIR variable
  # The variable is normally specified in my.cnf returning
  # the BackupDataDir value of the data nodes
}

# Directory where NDB will create backups
--let NDB_BACKUPS=$NDB_BACKUP_DIR/BACKUP/BACKUP
#echo NDB_BACKUPS: $NDB_BACKUPS;

# Directory containing backups from earlier versions of NDB
--let $NDB_SAVED_BACKUPS=$MYSQL_TEST_DIR/suite/ndb/backups
#echo NDB_SAVED_BACKUPS: $NDB_SAVED_BACKUPS;

--let NDB_RESTORE=$NDB_RESTORE --no-defaults

--enable_query_log
