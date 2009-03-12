################################################################################
# inc/partition_layout_check2.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#   Store the SHOW CREATE TABLE output and the list of files belonging to      #
#   this table + Check if the layout of the table was not modified             #
#   since the call of inc/partition_layout_check1.inc                          #
#   This script is only usefule when sourced within the partitioning tests.    #
#                                                                              #
# Attention: The routine inc/partition_layout_check1.inc is very similar   #
#            to this one. So if something has to be changed here it            #
#            might be necessary to do it also there                            #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
################################################################################

if ($no_debug)
{
--disable_query_log
}
# Clean the table holding the definition of t1
DELETE FROM t0_definition WHERE state = 'new';


# Get the MySQL Servers datadir without ending slash
let $MYSQLD_DATADIR= `select LEFT(@@datadir, LENGTH(@@datadir)-1)`;
#echo MYSQLD_DATADIR: $MYSQLD_DATADIR;

# Dump the current definition of the table t1 to tmp1
let $show_file= $MYSQLD_DATADIR/test/tmp1;
--exec echo "SHOW CREATE TABLE t1; exit; " | $MYSQL_TEST > $show_file 2>&1

if ($do_file_tests)
{
  # List the files belonging to the table t1
  let $ls_file= $MYSQLD_DATADIR/test/tmp2;
  --list_files_write_file $ls_file $MYSQLD_DATADIR/test t1*
  --chmod 0644 $ls_file
  if ($with_directories)
  {
    --list_files_append_file $ls_file $MYSQLTEST_VARDIR/mysql-test-data-dir t1*
    --list_files_append_file $ls_file $MYSQLTEST_VARDIR/mysql-test-idx-dir t1*
  }
  eval SET @aux = load_file('$ls_file');
}
if (!$do_file_tests)
{
  SET @aux = '--- not determined ---';
}

# Insert the current definition of the table t1 into t0_definition
eval INSERT INTO t0_definition SET state = 'new',
     create_command = load_file('$show_file'),
     file_list      = @aux;

# Print the old and new table layout, if they differ
SELECT COUNT(*) <> 1 INTO @aux
FROM t0_definition tab1, t0_definition tab2
WHERE tab1.state = 'old' AND tab2.state = 'new'
  AND tab1.create_command = tab2.create_command
  AND tab1.file_list = tab2.file_list;
let $run= `SELECT @aux`;
if ($run)
{
   --vertical_results
   --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
   SELECT state,
   REPLACE(create_command,'\n',' ') AS "Table definition",
   REPLACE(file_list     ,'\n',' ') AS "File list"
   FROM t0_definition WHERE state in ('old','new');
   --horizontal_results
   --echo # check layout success:    0
}
let $run= `SELECT @aux = 0`;
if ($run)
{
   --echo # check layout success:    1
}
--enable_query_log
