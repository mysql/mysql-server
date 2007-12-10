# inc/partition_layout.inc
#
# Print partitioning related informations about the table t1
#

eval SHOW CREATE TABLE t1;

# Optional (most probably issues with separators and case sensitivity)
# listing of files belonging to the table t1
if ($ls)
{
   --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
   --exec ls $MYSQLTEST_VARDIR/master-data/test/t1*
}
