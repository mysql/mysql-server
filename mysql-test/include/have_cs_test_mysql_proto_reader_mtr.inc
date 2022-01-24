
--disable_query_log
--let $temp_inc=$MYSQL_TMP_DIR/cs-test-mysql-proto-reader_exists.inc
--let inc_tmp=$temp_inc
--let $basedir=`SELECT @@basedir`
--let file_to_check=$basedir/bin/cs-test-mysql-proto-reader-mtr

--perl
use strict;
my $temp_inc= $ENV{'inc_tmp'} or die "temp_inc not set";
open(FILE_INC,">", "$temp_inc") or die("can't open file \"$temp_inc\": $!");
my $file_exists = 0;
if (-e $ENV{'file_to_check'}) {
    $file_exists=1;
}
print FILE_INC '--let $cs_test_mysql_proto_reader_mtr_file_exists=' . $file_exists;
close FILE_INC;
EOF

--source $temp_inc
--file_exists $temp_inc 5
--remove_file $temp_inc
--enable_query_log

if (!$cs_test_mysql_proto_reader_mtr_file_exists)
{
  --skip cs-test-mysql-proto-reader-mtr does not exist
}
