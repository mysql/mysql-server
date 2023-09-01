# test error message compiler

--source include/not_windows.inc

--echo # WL#13423: Split errmgs-utf8mb3.txt into one file for messages to clients and another for messages to the error log

--source include/have_perror.inc

--disable_query_log
eval SELECT "$MY_PERROR" INTO @perror;
let COMP_ERR=`SELECT CONCAT(REGEXP_SUBSTR(@perror, "^.*/"), "comp_err");`;
--exec $COMP_ERR --self-test=$MYSQLTEST_VARDIR/std_data/comp_err
--enable_query_log
