# 
# This is an include file that performs some assertions
# on behalf of rpl_row_img_sanity test case.
#
# It begins by dumping the current connection binary log
# and then compares it against the expected values (which
# are parameters to this include) 
#
# Expected values are arguments to this script and must be structured
# as follows:
#
#  -- let $row_img_expected= 1:1 2:2 3:'a' 4:NULL | 1:2 4:10
#
# This example means that BEFORE IMAGE contains values for columns
# with the given index in the original table 1, 2, 3, 4 and their
# values are 1,2,'a',NULL respectively.
#
# The same resoning for the AFTER IMAGE that follows the image
#  separator char '|'.
#
# Arguments:
#
#  - $row_img_expected 
#    The expected values for BI and AI that we are searching for
#  - $row_img_pos
#    The start position in the binary log from which the searching
#    will be done
#
# Sample usage:
#
#   -- let $row_img_expected= 1:1 2:2 3:'a' 4:NULL | 1:2 4:10
#   -- let $row_img_pos= 107
#   -- source include/rpl_row_img_parts_assertion.inc
#  

if (`SELECT LENGTH("$row_img_pos") = 0`)
{
  -- echo $row_img_pos not defined: $row_img_pos
  -- die Baiing out!
}

if (`SELECT LENGTH("$row_img_expected") = 0`)
{
  -- echo \$row_img_expected not defined: $row_img_expected
  -- die Baiing out!
}

-- let $_prefix= `SELECT UUID()`
-- let $TMP_FILE= $MYSQLTEST_VARDIR/tmp/$_prefix.tmp

-- let $binlog= query_get_value(SHOW MASTER STATUS, File, 1)
-- let $MYSQLD_DATADIR= `select @@datadir;`
-- exec $MYSQL_BINLOG -v --start-position=$row_img_pos $MYSQLD_DATADIR/$binlog > $TMP_FILE

-- let IMG_EXPECTED=$row_img_expected
-- let IMG_BINLOG_FILE= $TMP_FILE

-- perl END_OF_FILE

my $img = $ENV{'IMG_EXPECTED'};
my $file= $ENV{'IMG_BINLOG_FILE'};

open(FILE, $file) or die("Unable to open $binlog: $!\n");
my $contents = do { local $/; <FILE> };
close(FILE) or die("Unable to close file.");

# Save IMG_EXPECTED in $img and check if it has correct format
$img =~ /^([0-9]+:\S+ )* *\| *( [0-9]+:\S+)*$/ or \
        die "Invalid format of IMG_EXPECTED parameter. GOT: '$img'";

# Turn $img into the format of the binlog, and get BI and AI
$img =~ s/ *([0-9]+):(\S*) */###   \@$1=$2\n/g;
my ($bi, $ai)= split(/ *\| */, $img);
# Generate regular expression
if ($ai)
{
  if ($bi)
  {
    $pattern= "### UPDATE.*\n### WHERE\n$bi### SET\n$ai";
  }
  else
  {
    $pattern= "### INSERT.*\n### SET\n$ai";
  }
}
else
{
    $pattern= "### DELETE.*\n### WHERE\n$bi";
}
$match= ($contents =~ /$pattern/);

if (!$match)
{
  print "====================================================\n";
  print "PATTERN FOR EXPECTED IMAGES DID NOT MATCH:\n";
  print "====================================================\n";
  print "$pattern";
  print "====================================================\n\n";

  print "====================================================\n";
  print "BINLOG CONTENTS\n";
  print "====================================================\n";
  print "$contents";
  print "====================================================\n";
}

END_OF_FILE

-- let IMG_EXPECTED= 

-- remove_file $TMP_FILE

