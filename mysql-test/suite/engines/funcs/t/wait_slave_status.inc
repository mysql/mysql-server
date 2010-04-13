# include/wait_slave_status.inc
#
# Created by Matthias Leich
#
# SUMMARY
#
#    Waits until slave has reached certain state or maximum time reached.
#
#    (This script will not work, when the SHOW command delivers more than one 
#    result record, because only the first record will be caught.)
#
# USAGE
#
#    Set $result_pattern in test file and source this file:
#
#      let $result_pattern= <pattern used for LIKE on the result of
#                           SHOW STATUS SLAVE>
#      --include wait_slave_status.inc
#
# EXAMPLE
#
#    The script rpl_until.test:
#       ...
#       --replace_result $MASTER_MYPORT MASTER_MYPORT
#       --replace_column 1 # 9 # 23 # 33 #
#       --vertical_results show slave status;
#
#    outputs
#       show slave status;
#       Slave_IO_State  #
#       Master_Host     127.0.0.1
#       Master_User     root
#       Master_Port     MASTER_MYPORT
#       Connect_Retry   1
#       Master_Log_File master-bin.000001
#       Read_Master_Log_Pos     776
#       Relay_Log_File  slave-relay-bin.000004
#       Relay_Log_Pos   #
#       Relay_Master_Log_File   master-bin.000001
#       Slave_IO_Running        Yes
#       Slave_SQL_Running       No
#       Replicate_Do_DB
#       Replicate_Ignore_DB
#       Replicate_Do_Table
#       Replicate_Ignore_Table
#       Replicate_Wild_Do_Table
#       Replicate_Wild_Ignore_Table
#       Last_Errno      0
#       Last_Error
#       Skip_Counter    0
#       Exec_Master_Log_Pos     319
#       Relay_Log_Space #
#       Until_Condition Master
#       Until_Log_File  master-bin.000001
#       Until_Log_Pos   319
#       Master_SSL_Allowed      No
#       Master_SSL_CA_File
#       Master_SSL_CA_Path
#       Master_SSL_Cert
#       Master_SSL_Cipher
#       Master_SSL_Key
#       Seconds_Behind_Master   #
#
# The main problem with the "show slave status;" in rpl_until is, that
# depending on the total test engine power and the current load caused by
# other processes, the expected slave status might be not reached though
# it will happen in maybe some seconds.
#
# The typical problem with rpl_until is that Slave_IO_Running is "No"
# instead of "Yes".
#
# The expected result follows the LIKE pattern:
#
#   let $result_pattern= '%127.0.0.1%root%1%master-bin.000001%776%slave-relay-bin.000004%master-bin.000001%Yes%No%0%0%319%Master%master-bin.000001%319%No%';
#
# The Slave_IO_Running value is the "Yes" just after the "master-bin.000001".
#
# How to get this pattern ?
#
#     Any lines "--replace_result ..." and "--replace_colum ..." just before
#     the SHOW TABLE STATUS and of course the expected result itself
#     show us columns where the content must be unified, because it is non
#     deterministic or it depends on the current test environment.
#
#     Unfortunately "--replace_result ..." and "--replace_colum ..." do not
#     affect the result of our assignment    let $my_val= `SHOW SLAVE STATUS`;
#     Therefore such content must be covered by '%'.
#
#     Please be careful. A more simple pattern might be dangerous, because we
#     might get "wrong" matches. Example: There might be several "Yes" and "No"
#     within one result row.
#
###############################################################################

# We do not want to print the auxiliary commands, because they are not of
# interest and their amount will vary depending how fast we get the
# desired state.
--disable_query_log

# The protocol should show
# - the setting of $result_pattern   and
# - that this file is sourced           ,
# because this increases the chance to use the protocol as replay script.
eval SELECT "let \$result_pattern= $result_pattern ;" AS "";
SELECT '--source include/wait_slave_status.inc' AS "";

let $show_type= SLAVE STATUS;
let $show_pattern= $result_pattern;
--enable_query_log

-- source $MYSQL_TEST_DIR/suite/engines/funcs/t/wait_show_pattern.inc

if (!$success)
{
let $message= ! Attention: Timeout in wait_slave_status.inc.
              |       Possible reasons with decreasing probability:
              |       - The LIKE pattern is wrong, because the
              |         testcase was altered or the layout of the
              |         SHOW SLAVE STATUS result set changed.
              |       - There is a new bug within the replication.
              |       - We met an extreme testing environment and timeout is
              |         too small.;
--source include/show_msg80.inc
--echo DEBUG INFO START (wait_slave_status.inc):
--echo $result_pattern
--vertical_results
show slave status;
--echo DEBUG INFO END
}
