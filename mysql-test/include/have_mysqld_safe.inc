#################################################
# Checks if mysqld_safe is installed in the path
# specified by MTR. mysqld_safe is obsolete on
# platforms that use systemd to monitor mysqld.
#################################################

--disable_query_log
--let $temp_inc=$MYSQL_TMP_DIR/tmp.inc
let inc_tmp=$temp_inc;

--perl
use strict;
my $is_mysqld_safe_present=0;
$is_mysqld_safe_present=1 if -e $ENV{'MYSQLD_SAFE'};

my $temp_inc= $ENV{'inc_tmp'} or die "temp_inc not set";
open(FILE_INC, ">", "$temp_inc") or die("can't open file \"$temp_inc\": $!");
print FILE_INC '--let $is_found= '.$is_mysqld_safe_present;

EOF

--source $temp_inc

if (!$is_found)
{
--skip Tests using mysqld_safe skipped.
}

--remove_file $temp_inc
--enable_query_log
