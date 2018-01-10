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

my $NDBINFO_SELECT_ALL =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndbinfo_select_all");

my $NDB_DELETE_ALL =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_delete_all");

my $NDB_BLOB_TOOL =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_blob_tool");

my $NDB_DROP_INDEX =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_drop_index");


my $NDB_SELECT_COUNT =  my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_select_count");
my $NDB_PRINT_BACKUP_FILE = my_find_bin($bindir,
                           ["runtime_output_directory", "bin"],
                           "ndb_print_backup_file");


my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR" ;
open ( OUTPUT, ">$vardir/tmp/ndb_find_tools_result.inc") ;
print OUTPUT "let \$NDBINFO_SELECT_ALL=$NDBINFO_SELECT_ALL;\n";
print OUTPUT "let \$NDB_DELETE_ALL=$NDB_DELETE_ALL;\n";
print OUTPUT "let \$NDB_BLOB_TOOL=$NDB_BLOB_TOOL;\n";
print OUTPUT "let \$NDB_DROP_INDEX=$NDB_DROP_INDEX;\n";
print OUTPUT "let \$NDB_SELECT_COUNT=$NDB_SELECT_COUNT;\n";
print OUTPUT "let \$NDB_PRINT_BACKUP_FILE=$NDB_PRINT_BACKUP_FILE;\n";
close (OUTPUT);
EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_find_tools_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/ndb_find_tools_result.inc
