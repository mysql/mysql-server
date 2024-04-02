## Script test_dir.inc
#


--perl
use File::Basename;
use File::Spec::Functions;
my $mysql_test_file = $ENV{MYSQLTEST_FILE} or die "Need MYSQLTEST_FILE";
my $basedir = dirname($mysql_test_file);

my $tmpdir = $ENV{MYSQL_TMP_DIR} or die "Need $MYSQL_TMP_DIR";
open(FILE, ">", "$tmpdir/test_dir.tmp.inc") or die;
print FILE "--let \$TEST_DIR= $basedir\n";
close(FILE);
EOF

--source $MYSQL_TMP_DIR/test_dir.tmp.inc
--remove_file $MYSQL_TMP_DIR/test_dir.tmp.inc

