#######################################################
# checks if mysql is debug compiled.
# This "cannot" be done simply by using have_debug.inc
######################################################

--disable_query_log
--let $temp_out_help_file=$MYSQL_TMP_DIR/mysql_help.tmp
--exec $MYSQL --help>$temp_out_help_file
let log_tmp=$temp_out_help_file;
--let $temp_inc=$MYSQL_TMP_DIR/temp.inc
let inc_tmp=$temp_inc;

--perl
use strict;
my $tmp_file= $ENV{'log_tmp'} or die "log_tmp not set";
open(FILE, "$tmp_file") or die("Unable to open $tmp_file: $!\n");
my $count = () = grep(/Output debug log/g,<FILE>);
close FILE;

my $temp_inc= $ENV{'inc_tmp'} or die "temp_inc not set";
open(FILE_INC,">", "$temp_inc") or die("can't open file \"$temp_inc\": $!");
print FILE_INC '--let $is_debug= '.$count;
close FILE_INC;
EOF
--source $temp_inc

if (!$is_debug)
{
  --skip mysql needs to be debug compiled
}
--remove_file $temp_out_help_file
--remove_file $temp_inc
--enable_query_log
