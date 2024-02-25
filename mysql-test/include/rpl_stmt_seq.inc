################### include/rpl_stmt_seq.inc ###########################
#                                                                      #
# Check if a given SQL statement (->$my_stmt) / AUTOCOMMIT mode /      #
# storage engine somehow involved causes COMMIT or ROLLBACK.           #
#                                                                      #
#                                                                      #
# The typical test sequence                                            #
# -------------------------                                            #
# 1. master connection: INSERT without commit                          #
#    check table content of master and slave                           #
# 2. master connection: EXECUTE the statement                          #
#    check table content of master and slave                           #
# 3. master connection: ROLLBACK                                       #
#    check table content of master and slave                           #
# 4. flush the logs                                                    #
#                                                                      #
# The variables                                                        #
#    $show_binlog       -- print binlog entries                        #
#                          0 - default + fits to the file with         #
#                              results                                 #
#                          1 - useful for debugging                    #
#                          This variable is used within                #
#                          include/rpl_stmt_seq.inc.                   #
#    $manipulate        -- Manipulation of the binary logs             #
#                          0 - do nothing                              #
#                          1 - so that the output of SHOW BINLOG       #
#                              EVENTS IN <current log> contains only   #
#                              commands of the current test sequence   #
#                              This is especially useful, if the       #
#                              $show_binlog is set to 1 and many       #
#                              subtest are executed.                   #
#                          This variable is used within                #
#                          include/rpl_stmt_seq.inc.                   #
# have to be set before sourcing this script.                          #
#                                                                      #
# Please be very careful when editing this routine, because the        #
# handling of the $variables is extreme sensitive.                     #
#                                                                      #
########################################################################

# Last update:
# 2007-02-12 ML Replace comments via SQL by "--echo ..."
#

let $VERSION=`select version()`;

--echo
--echo ######## $my_stmt ########


###############################################################
# Predict the number of the current log
###############################################################
# Disable the logging of the log number computation. 
--disable_query_log
# $_log_num_n should contain the number of the current binlog in numeric style.
# If this routine is called for the first time, $_log_num will not initialized
# and contain the value '' instead of '1'. So we will correct it here.
#
eval set @aux= IF('$_log_num_n' = '', '1', '$_log_num_n');
let $_log_num_n= `SELECT @aux`;
eval set @aux= LPAD('$_log_num_n',6,'0');
#              SELECT @aux AS "@aux is";
#
# $_log_num_s should contain the number of the current binlog in string style.
let $_log_num_s= `select @aux`;
# eval SELECT '$log_num' ;
--enable_query_log

###############################################################
# INSERT 
###############################################################
--echo
--echo -------- switch to master -------
connection master;
# Maybe it would be smarter to use a table with an autoincrement column.
let $MAX= `SELECT MAX(f1) FROM t1` ;
eval INSERT INTO t1 SET f1= $MAX + 1;
# results before DDL(to be tested)
SELECT MAX(f1) FROM t1;
if ($show_binlog)
{
  --let $binlog_file= master-bin.$_log_num_s
  --source include/show_binlog_events.inc
}
sync_slave_with_master;

--echo
--echo -------- switch to slave --------
connection slave;
# results before DDL(to be tested)
SELECT MAX(f1) FROM t1;
if ($show_binlog)
{
  --let $binlog_file= slave-bin.$_log_num_s
  --source include/show_binlog_events.inc
}

###############################################################
# command to be tested
###############################################################
--echo
--echo -------- switch to master -------
connection master;
eval $my_stmt;
# Devaluate $my_stmt, to detect script bugs
let $my_stmt= ERROR: YOU FORGOT TO FILL IN THE STATEMENT;
# results after DDL(to be tested)
SELECT MAX(f1) FROM t1;
if ($show_binlog)
{
  --let $binlog_file= master-bin.$_log_num_s
  --source include/show_binlog_events.inc
}
sync_slave_with_master;

--echo
--echo -------- switch to slave --------
connection slave;
# results after DDL(to be tested)
SELECT MAX(f1) FROM t1;
if ($show_binlog)
{
  --let $binlog_file= slave-bin.$_log_num_s
  --source include/show_binlog_events.inc
}

###############################################################
# ROLLBACK
###############################################################
--echo
--echo -------- switch to master -------
connection master;
ROLLBACK;
# results after final ROLLBACK
SELECT MAX(f1) FROM t1;
# Try to detect if the DDL command caused that the INSERT is committed
# $MAX holds the highest/last value just before the insert of MAX + 1
--disable_query_log
eval SELECT CONCAT(CONCAT('TEST-INFO: MASTER: The INSERT is ',
               IF(MAX(f1) = $MAX + 1, 'committed', 'not committed')),
               IF((MAX(f1) = $MAX + 1) XOR NOT $my_master_commit,
                ' (Succeeded)',
                ' (Failed)')) AS "" 
               FROM mysqltest1.t1;
--enable_query_log
if ($show_binlog)
{
  --let $binlog_file= master-bin.$_log_num_s
  --source include/show_binlog_events.inc
}
sync_slave_with_master;

--echo
--echo -------- switch to slave --------
connection slave;
# results after final ROLLBACK
SELECT MAX(f1) FROM t1;
--disable_query_log
eval SELECT CONCAT(CONCAT('TEST-INFO: SLAVE:  The INSERT is ',
               IF(MAX(f1) = $MAX + 1, 'committed', 'not committed')),
               IF((MAX(f1) = $MAX + 1) XOR NOT $my_slave_commit,
                ' (Succeeded)',
                ' (Failed)')) AS "" 
               FROM mysqltest1.t1;
--enable_query_log
if ($show_binlog)
{
  --let $binlog_file= slave-bin.$_log_num_s
  --source include/show_binlog_events.inc
}

###############################################################
# Manipulate binlog
###############################################################
if ($manipulate)
{
#### Manipulate the binary logs,
# so that the output of SHOW BINLOG EVENTS IN <current log>
# contains only commands of the current test sequence.
# - flush the master and the slave log
#   ---> both start to write into new logs with incremented number
# - increment $_log_num_n
--echo
--echo -------- switch to master -------
connection master;
flush logs;
# sleep 1;
# eval SHOW BINLOG EVENTS IN 'master-bin.$_log_num_s';
sync_slave_with_master;

--echo
--echo -------- switch to slave --------
connection slave;
# the final content of the binary log
flush logs;
# The next sleep is urgent needed.
# Without this sleep the slaves crashes often, when the SHOW BINLOG
# is executed.   :-(
# sleep 1;
# eval SHOW BINLOG EVENTS IN 'slave-bin.$_log_num_s';
inc $_log_num_n;
}

--echo
--echo -------- switch to master -------
connection master;
