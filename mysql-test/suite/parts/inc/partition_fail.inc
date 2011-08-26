# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--disable_abort_on_error
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

--echo # Same test under LOCK TABLE
--eval $create_statement
--eval $insert_statement
--echo # State before failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
LOCK TABLE t1 WRITE;
--disable_abort_on_error
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
UNLOCK TABLES;
DROP TABLE t1;
