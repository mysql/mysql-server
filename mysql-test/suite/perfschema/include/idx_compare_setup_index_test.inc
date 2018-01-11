#
# Performance Schema, Index validation
#

--echo
--echo ## Create index_test() procedure
--disable_warnings
DROP PROCEDURE IF EXISTS index_test;
--enable_warnings


--disable_query_log

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

--enable_query_log

