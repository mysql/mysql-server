################################################################################
# inc/partition_layout_check1.inc                                          #
#                                                                              #
# Purpose:                                                                     #
#   Store the SHOW CREATE TABLE output and the list of files belonging to      #
#   this table + print this into the protocol                                  #
#   This script is only usefule when sourced within the partitioning tests.    #
#                                                                              #
# Attention: The routine inc/partition_layout_check2.inc is very similar   #
#            to this one. So if something has to be changed here it            #
#            might be necessary to do it also there                            #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

if ($no_debug)
{
--disable_query_log
}
# Clean the table holding the definition of t1
DELETE FROM t0_definition;

# Dump the current definition of the table t1 to tmp1
# This complicated method - let another mysqltest collect the output - is used
# because of two reasons
# - SHOW CREATE TABLE t1 is at least currently most probably more reliable than
#   the corresponding SELECT on the INFORMATION_SCHEMA
# - SHOW CREATE TABLE .. cannot write its out put into a file like SELECT
--exec echo "SHOW CREATE TABLE t1;" | $MYSQL_TEST > $MYSQLTEST_VARDIR/master-data/test/tmp1 2>&1 || true
if ($do_file_tests)
{
# List the files belonging to the table t1
--exec ls $MYSQLTEST_VARDIR/master-data/test/t1* > $MYSQLTEST_VARDIR/master-data/test/tmp2 2>&1 || true
if ($with_directories)
{
--exec ls $MYSQLTEST_VARDIR/master-data/test/data/t1* >> $MYSQLTEST_VARDIR/master-data/test/tmp2 2>&1 || true
--exec ls $MYSQLTEST_VARDIR/master-data/test/index/t1* >> $MYSQLTEST_VARDIR/master-data/test/tmp2 2>&1 || true
}
eval SET @aux = CONCAT('load_file(''$MYSQLTEST_VARDIR','/master-data/test/tmp2''
)');
let $file_list= `SELECT @aux`;
}
if (!$do_file_tests)
{
let $file_list= '--- not determined ---';
}

# Insert the current definition of the table t1 into t0_definition
eval INSERT INTO t0_definition SET state = 'old',
     create_command = load_file('$MYSQLTEST_VARDIR/master-data/test/tmp1'),
     file_list      = $file_list;

# Print the create table statement into the protocol
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
SELECT create_command FROM t0_definition WHERE state = 'old';
if ($do_file_tests)
{
   # We stored the list of files, therefore printing the content makes sense
   if ($ls)
   {
      # Print the list of files into the protocol
      eval SELECT REPLACE(file_list,'$MYSQLTEST_VARDIR','\$MYSQLTEST_VARDIR')
                  AS "unified filelist"
           FROM t0_definition WHERE state = 'old';
   }
}
--enable_query_log
