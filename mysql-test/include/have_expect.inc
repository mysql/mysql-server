###############################################
# checks if the perl expect module 
# is installed on the system where
# the test is run
###############################################

--let $tmp_inc=$MYSQL_TMP_DIR/tmp.inc
let inc_tmp=$tmp_inc;

--perl

my $tmp_inc= $ENV{'inc_tmp'} or die "tmp_inc not set";
my $is_module_present = eval
{
  require Expect;
};

open(FILE_INC, ">" , "$tmp_inc") or die("Unable to open $tmp_inc: $!\n");
print FILE_INC '--let $is_present= '.$is_module_present;
close FILE_INC;

EOF

--source $tmp_inc

if (!$is_present)
{
  --skip Please install the perl Expect module
}

--remove_file $tmp_inc
