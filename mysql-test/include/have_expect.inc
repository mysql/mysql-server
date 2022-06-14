###############################################
# checks if the perl expect module 
# is installed on the system where
# the test is run
###############################################

--let tmp_inc=$MYSQL_TMP_DIR/tmp.inc

--perl

use strict;
use warnings;

my $tmp_inc= $ENV{'tmp_inc'} or die('Env. variable $tmp_inc is not set');
my $ret = 1;
my $is_module_present = eval
{
  require Expect;
};

if (!$is_module_present) {
  $ret = 0;
}

open(FILE_INC, ">" , "$tmp_inc") or die("Unable to open $tmp_inc: $!\n");
print FILE_INC 'let $is_present= ' . $ret . ";\n";
close FILE_INC;

EOF

--source $tmp_inc
--remove_file $tmp_inc
if (!$is_present)
{
  --skip Please install the perl Expect module
}
