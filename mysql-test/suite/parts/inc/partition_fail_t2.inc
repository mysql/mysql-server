# Include file to to test failure with error injection.
# To be used with WL#4445: EXCHANGE PARTITION WITH TABLE.
--eval $create_statement2
--eval $insert_statement2
SHOW CREATE TABLE t2;
--sorted_result
SELECT * FROM t2;
--eval $create_statement
--eval $insert_statement
--echo # State before failure
--replace_result #p# #P#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
# accept all errors
--disable_abort_on_error
--replace_regex /#sqlx-[0-9a-f_]*/#sqlx-nnnn_nnnn/i
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--replace_result #p# #P#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
SHOW CREATE TABLE t2;
--sorted_result
SELECT * FROM t2;
DROP TABLE t2;
