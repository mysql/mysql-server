# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before failure
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--disable_abort_on_error
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
