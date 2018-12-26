call mtr.add_suppression("Could not use");
# Case 1: Setting fife file to general_log_file and slow_query_log_file
#         system variable.
SET GLOBAL general_log_file="MYSQLTEST_VARDIR/tmp/general_log.fifo";;
ERROR 42000: Variable 'general_log_file' can't be set to the value of 'MYSQLTEST_VARDIR/tmp/general_log.fifo'
SET GLOBAL slow_query_log_file="MYSQLTEST_VARDIR/tmp/slow_log.fifo";;
ERROR 42000: Variable 'slow_query_log_file' can't be set to the value of 'MYSQLTEST_VARDIR/tmp/slow_log.fifo'
# Case 2: Starting server with fifo file as general log file
#         and slow query log file.
Setting fifo file as general log file and slow query log failed.
