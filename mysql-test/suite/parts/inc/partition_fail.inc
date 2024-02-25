# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
--disable_abort_on_error
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
DROP TABLE `t1/a\b#2`;

--echo # Same test under LOCK TABLE
--eval $create_statement
--eval $insert_statement
--echo # State before failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
LOCK TABLE `t1/a\b#2` WRITE;
--disable_abort_on_error
--eval $fail_statement
--enable_abort_on_error
--echo # State after failure
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
UNLOCK TABLES;
DROP TABLE `t1/a\b#2`;
