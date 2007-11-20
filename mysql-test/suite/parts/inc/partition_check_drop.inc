################################################################################
# inc/partition_check_drop.inc                                                 #
#                                                                              #
# Purpose:                                                                     #
#   Check that a drop table removes all files belonging to this table.         #
#   Remaining unused files can be caused by imperfect DROP TABLE or            #
#   ALTER TABLE <alter partitioning>.                                          #
#                                                                              #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-05-12                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

if ($no_debug)
{
   --disable_query_log
}

if ($do_file_tests)
{
# List the files belonging to the table t1
--exec ls $MYSQLTEST_VARDIR/master-data/test/t1* > $MYSQLTEST_VARDIR/master-data/test/tmp2 || true
if ($with_directories)
{
--exec ls $MYSQLTEST_VARDIR/master-data/test/data/t1* >> $MYSQLTEST_VARDIR/master-data/test/tmp2 || true
--exec ls $MYSQLTEST_VARDIR/master-data/test/index/t1* >> $MYSQLTEST_VARDIR/master-data/test/tmp2 || true
}
eval SET @aux = CONCAT('load_file(''$MYSQLTEST_VARDIR','/master-data/test/tmp2'')');
let $file_list= `SELECT @aux`;
}
if (!$do_file_tests)
{
let $file_list= '--- not determined ---';
}

# UPDATE the current filelist of the table t1 within t0_definition
# Note: This list should be empty, because the table t1 was dropped !
eval INSERT INTO t0_definition SET state = 'old', file_list = $file_list
ON DUPLICATE KEY UPDATE file_list = $file_list;
# eval UPDATE t0_definition SET file_list = $file_list WHERE state = 'old';

# Check if filelist is empty.
let $found_garbage= `SELECT file_list <> '' FROM t0_definition WHERE state = 'old'`;
if ($found_garbage)
{
   # Unfortunately the DROP TABLE did not remove the unused files
   if ($ls)
   {
      --echo # Attention: There are unused files.
      --echo #            Either the DROP TABLE or a preceding ALTER TABLE
      --echo #            <alter partitioning> worked incomplete.
      --echo # We found:
      # Print the list of files into the protocol
      eval SELECT REPLACE(file_list,'$MYSQLTEST_VARDIR','\$MYSQLTEST_VARDIR')
                  AS "unified filelist"
           FROM t0_definition WHERE state = 'old';
   }
   # Do a manual cleanup, because the following tests should not suffer from
   # remaining files
   --exec rm -f $MYSQLTEST_VARDIR/master-data/test/t1* || true
   if ($with_directories)
   {
	   --exec rm -f $MYSQLTEST_VARDIR/master-data/test/data/t1* || true
	   --exec rm -f $MYSQLTEST_VARDIR/master-data/test/index/t1* || true
   }
}
--enable_query_log
