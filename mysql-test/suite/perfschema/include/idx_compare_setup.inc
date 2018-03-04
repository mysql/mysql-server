#
# Performance Schema, Index validation
#
--echo
--echo ===== SETUP =====
let $get_thread_id=
  SELECT thread_id INTO @my_thread_id
  FROM performance_schema.threads
  WHERE processlist_id = connection_id();

let $get_thread_os_id=
  SELECT thread_os_id INTO @my_thread_os_id
  FROM performance_schema.threads
  WHERE processlist_id = connection_id();

let $get_processlist_id=
  SELECT connection_id() INTO @my_processlist_id;

--disable_query_log

--connection default
--echo ## Connection Default
--echo ## Disable events from the default connection
UPDATE performance_schema.threads SET instrumented = 'NO' WHERE processlist_id = CONNECTION_ID();

## Remove residual entries in the tables performance_schema.users
## and performance_schema.accounts
TRUNCATE TABLE performance_schema.users;
TRUNCATE TABLE performance_schema.accounts;

## Get the thread_id of the default connection
eval $get_thread_id;
let $default_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $default_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $default_processlist_id= `SELECT @my_processlist_id`;

USE test;

--enable_query_log

--echo
--echo ## Create user1, user2, user3
CREATE USER user1@localhost;
CREATE USER user2@localhost;
CREATE USER user3@localhost;
GRANT ALL ON *.* to 'user1'@localhost;
GRANT ALL ON *.* to 'user2'@localhost;
GRANT ALL ON *.* to 'user3'@localhost;

--disable_query_log

--echo
--echo ## Setup test connection 1
connect(con1, localhost, user1,,);
## Get the thread_id of connection 1
eval $get_thread_id;
let $con1_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con1_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con1_processlist_id= `SELECT @my_processlist_id`;

--echo ## Setup test connection 2
connect(con2, localhost, user2,,);
## Get the thread_id of connection 2
eval $get_thread_id;
let $con2_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con2_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con2_processlist_id= `SELECT @my_processlist_id`;

--echo ## Setup test connection 3
connect(con3, localhost, user3,,);
## Get the thread_id of connection 3
eval $get_thread_id;
let $con3_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con3_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con3_processlist_id= `SELECT @my_processlist_id`;

--enable_query_log

--echo
--echo ## Default connection
--connection default

--echo
--echo ## Create data tables
CREATE TABLE test.t1(a INT, b INT, c INT, d INT DEFAULT 0, PRIMARY KEY(a), INDEX index_b(b), INDEX index_cb(c, b));
CREATE TABLE test.t2 LIKE test.t1;
CREATE TABLE test.t3 LIKE test.t1;

--disable_query_log
--disable_result_log

--echo
--echo ## Generate event data from 3 connections

--connection con1
--echo ## Connection 1
SET @uvar_temp = 1;
INSERT INTO test.t1 SET a=101, b=1, c=1;
INSERT INTO test.t2 SET a=102, b=2, c=2;
INSERT INTO test.t2 SET a=103, b=3, c=3;
INSERT INTO test.t3 SET a=104, b=4, c=4;
INSERT INTO test.t3 SET a=105, b=5, c=5;
INSERT INTO test.t3 SET a=106, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=101;
UPDATE test.t2 SET d=d+1 WHERE a=101;
UPDATE test.t3 SET d=d+1 WHERE a=101;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--connection con2
--echo ## Connection 2
SET @uvar_temp = 2;
INSERT INTO test.t1 SET a=201, b=1, c=1;
INSERT INTO test.t2 SET a=202, b=2, c=2;
INSERT INTO test.t2 SET a=203, b=3, c=3;
INSERT INTO test.t3 SET a=204, b=4, c=4;
INSERT INTO test.t3 SET a=205, b=5, c=5;
INSERT INTO test.t3 SET a=206, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=201;
UPDATE test.t2 SET d=d+1 WHERE a=201;
UPDATE test.t3 SET d=d+1 WHERE a=201;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--connection con3
--echo ## Connection 3
SET @uvar_temp = 3;
INSERT INTO test.t1 SET a=301, b=1, c=1;
INSERT INTO test.t2 SET a=302, b=2, c=2;
INSERT INTO test.t2 SET a=303, b=3, c=3;
INSERT INTO test.t3 SET a=304, b=4, c=4;
INSERT INTO test.t3 SET a=305, b=5, c=5;
INSERT INTO test.t3 SET a=306, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=301;
UPDATE test.t2 SET d=d+1 WHERE a=301;
UPDATE test.t3 SET d=d+1 WHERE a=301;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--enable_result_log
--enable_query_log

--connection con2
--echo ## Connection 2
--echo ## Create prepared statements on connection 2
PREPARE st1 FROM 'SELECT SQRT(POW(?,2) + POW(?,2)) AS hypotenuse';
PREPARE st3 FROM 'INSERT INTO test.t1 SELECT * FROM test.t1 WHERE a<=?';
PREPARE st4 FROM '(SELECT a FROM test.t1) UNION (SELECT a+10 FROM test.t1) ORDER BY RAND()*0+a';

--echo ## Create stored procedure connection 2
DELIMITER |;
CREATE PROCEDURE SampleProc1()
BEGIN
  SET @table = 'test.t1';
  SET @s = CONCAT('SELECT * FROM ', @table);
  PREPARE st2 FROM @s;
END|
DELIMITER ;|

--echo
--echo Call SampleProc1() to prepare a statement
CALL SampleProc1();

--disable_query_log
--disable_result_log

## Generate a system error
--error ER_NO_SUCH_TABLE
SELECT * from test.no_table;

--enable_result_log

#DEBUG --echo
#DEBUG SELECT * FROM performance_schema.prepared_statements_instances;

--echo ## Connection default
--connection default

# Set default column names
SET @pfs_columns = "*";
let $target_host = localhost;

--echo
--echo ## Use Connection 2 for default target thread id
eval SET @target_thread_id= $con2_thread_id;
eval SET @default_thread_id= $default_thread_id;

# DEBUG
#--echo ### Default thread: $default_thread_id
#--echo ### Con1 thread id: $con1_thread_id
#--echo ### Con2 thread id: $con2_thread_id (target)
#--echo ### Con3 thread id: $con3_thread_id

--echo
--echo ## Create index_test() procedure
--disable_warnings
DROP PROCEDURE IF EXISTS index_test;
--enable_warnings

# This procedure executes a SELECT for the given table, once with indexes disabled
# and again with indexes enabled. The two result sets are compared.
#
# 1. Run query with no indexes "USE INDEX ()".
# 2. Save results in temporary table, prefix rows with sequence number.
# 3. Run queries with indexes enabled.
# 4. Save results in temporary table, prefix rows with sequence number.
# 5. Compare tables for extra or missing rows with a JOIN on the sequence number.
#
# Parameters:
#    table_name    - Name of table
#    where_clause  - WHERE clause with indexes

DELIMITER $$;
CREATE PROCEDURE index_test(IN table_name VARCHAR(1024),
                            IN where_clause VARCHAR(1024))
BEGIN
  DROP TABLE IF EXISTS test.indexes_off;
  SET @table_name = table_name;
  SET @where_clause = where_clause;

  # Tables such as events_errors with TIMESTAMP columns, using "*" for the
  # result set tables fails with "1067: Invalid default value for 'FIRST_SEEN'".
  # As a workaround, specify only the necessary columns.

  ## Indexes disabled ##
 
  # Create table from result set with autoincremented sequence for each row (use AUTOINC?)
  SET @row:=0;
  SET @query_no_idx = CONCAT("SELECT ", @pfs_columns," FROM performance_schema.", @table_name, " USE INDEX () ", @where_clause);
  SET @main_query = CONCAT("CREATE TABLE test.indexes_off AS (SELECT @row:=@row+1 SEQ, output.* FROM (", @query_no_idx, ") output);");
  PREPARE stmt1 FROM @main_query;
  EXECUTE stmt1;
  DEALLOCATE PREPARE stmt1;
  # Index the sequence column -- needed for large joins
  ALTER TABLE test.indexes_off ADD INDEX (seq);

  ## Indexes enabled ##

  # Record the initial handler_read_key count
  FLUSH STATUS;
  SELECT variable_value INTO @read_key_before FROM performance_schema.session_status USE INDEX() WHERE variable_name = "handler_read_key";
  SET @read_key_before := @read_key_before + 0;

  DROP TABLE IF EXISTS test.indexes_on;
  SET @row:=0;
  SET @query = CONCAT("SELECT ", @pfs_columns," FROM performance_schema.", @table_name, " ", @where_clause);
  SET @main_query = CONCAT("CREATE TABLE test.indexes_on AS (SELECT @row:=@row+1 SEQ, output.* FROM (", @query, ") output);");
  SET @explain = CONCAT("EXPLAIN ", @query);
  SELECT @query AS "========";
  PREPARE stmt2 FROM @main_query;
  EXECUTE stmt2;
  DEALLOCATE PREPARE stmt2;
  
  # Record the final handler_read_key count
  SELECT variable_value INTO @read_key_after FROM performance_schema.session_status USE INDEX() WHERE variable_name = "handler_read_key";
  SET @read_key_after := @read_key_after + 0;
  
  # Index the sequence column -- needed for large joins
  ALTER TABLE test.indexes_on ADD INDEX (seq);

  #PREPARE stmt3 FROM @explain;
  #EXECUTE stmt3;
  #DEALLOCATE PREPARE stmt3;

  #DEBUG
  #SELECT * FROM test.indexes_off;
  #SELECT * FROM test.indexes_on;

  ## Verify that the indexes were used
  
  IF (@read_key_before >= @read_key_after) THEN
    SET @msg = CONCAT('*** ERROR *** Handler_read_key before: ', @read_key_before, '  After: ', @read_key_after);
  ELSE
    SET @msg = "OK: Handler_read_key";
    #DEBUG SET @msg = CONCAT('OK: Handler_read_key before: ', @read_key_before, '  After: ', @read_key_after);
  END IF;
  SELECT @msg AS "";

  ## Verify that the expected result set was generated

  SELECT COUNT(*) > 0 INTO @expected FROM test.indexes_off;
  SELECT COUNT(*) > 0 INTO @actual FROM test.indexes_on;

  IF @expected = 1 THEN
    SET @result_set = "Non-empty";
    SET @bad_result_set = "Empty";
  ELSE
    SET @result_set = "Empty";
    SET @bad_result_set = "Non-empty";
  END IF;

  IF @actual != @expected THEN
    SET @result = CONCAT('*** ERROR ***: Result set: ', @bad_result_set, '  Expected: ', @result_set);
  ELSE
    SET @result = CONCAT('OK: Result set: ', @result_set, '  Expected: ', @result_set);
  END IF;

  SELECT @result AS "";

  ## Check for extra or missing rows

  SELECT COUNT(*) INTO @extra_rows FROM test.indexes_off indexes_off LEFT JOIN test.indexes_on ON (indexes_off.seq = indexes_on.seq) WHERE indexes_on.seq IS NULL;
  SELECT COUNT(*) INTO @missing_rows FROM test.indexes_on indexes_on RIGHT JOIN test.indexes_off ON (indexes_on.seq = indexes_off.seq) WHERE indexes_off.seq IS NULL;

  SET @result = "OK: ";
  IF (@extra_rows > 0) OR (@missing_rows > 0) THEN
    SET @result = "*** ERROR ***: ";
  END IF;
  SET @rows = CONCAT(@result, 'Extra rows: ', @extra_rows, '  Missing rows: ', @missing_rows);
  SELECT @rows AS "";

  ## Compare checksums
  #CHECKSUM TABLE test.indexes_off;
  #CHECKSUM TABLE test.indexes_on;

  END$$
DELIMITER ;$$

## Method to compare checksums
# let $check1 = query_get_value(checksum table test.indexes_on, Checksum, 1);
# let $check2 = query_get_value(checksum table test.indexes_off, Checksum, 1);
# if ($check1 != $check2)
# {
#   --echo *** ERROR *** Checksum mismatch
#   SELECT * FROM test.indexes_off;
#   SELECT * FROM test.indexes_on;
# }
# if ($check1 == $check2)
# {
#   --echo OK: Checksums match
# }

--enable_query_log
