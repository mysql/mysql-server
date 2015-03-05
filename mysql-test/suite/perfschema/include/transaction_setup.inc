# Tests for the performance schema

# ==========================================
# HELPER include/transaction_setup.inc
# ==========================================

#
# UTILITY QUERIES
#
let $get_thread_id=
  SELECT thread_id INTO @my_thread_id
  FROM performance_schema.threads
  WHERE processlist_id = connection_id();

let $disable_instruments=
  UPDATE performance_schema.setup_instruments
  SET enabled='no', timed='no'
  WHERE name IN ('transaction');

let $enable_instruments=
  UPDATE performance_schema.setup_instruments
  SET enabled='yes', timed='yes'
  WHERE name IN ('transaction');

let $def_count = -1;

--disable_warnings
DROP PROCEDURE IF EXISTS clear_transaction_tables;
--enable_warnings

--disable_result_log

DELIMITER $$;
CREATE PROCEDURE clear_transaction_tables()
BEGIN
  truncate table performance_schema.events_transactions_current;
  truncate table performance_schema.events_transactions_history;
  truncate table performance_schema.events_transactions_history_long;
  truncate table performance_schema.events_transactions_summary_by_thread_by_event_name;
  truncate table performance_schema.events_transactions_summary_by_account_by_event_name;
  truncate table performance_schema.events_transactions_summary_by_host_by_event_name;
  truncate table performance_schema.events_transactions_summary_by_user_by_event_name;
  truncate table performance_schema.events_transactions_summary_global_by_event_name;
END$$

CREATE PROCEDURE clear_transaction_history()
BEGIN
  truncate table performance_schema.events_transactions_current;
  truncate table performance_schema.events_transactions_history;
  truncate table performance_schema.events_transactions_history_long;
END$$

CREATE PROCEDURE clear_statement_history()
BEGIN
  truncate table performance_schema.events_statements_current;
  truncate table performance_schema.events_statements_history;
  truncate table performance_schema.events_statements_history_long;
END$$

CREATE PROCEDURE clear_history()
BEGIN
  truncate table performance_schema.events_statements_current;
  truncate table performance_schema.events_statements_history;
  truncate table performance_schema.events_statements_history_long;
  truncate table performance_schema.events_transactions_current;
  truncate table performance_schema.events_transactions_history;
  truncate table performance_schema.events_transactions_history_long;
END$$

CREATE PROCEDURE transaction_verifier(IN i_table INT,
                                      IN i_thread_id INT,
                                      IN i_event_name VARCHAR(64),
                                      IN i_state VARCHAR(32),
                                      IN i_xid_format_id INT,
                                      IN i_xid_gtrid VARCHAR(130),
                                      IN i_xid_bqual VARCHAR(130),
                                      IN i_xa_state VARCHAR(64),
                                      IN i_gtid VARCHAR(64),
                                      IN i_access_mode VARCHAR(32),
                                      IN i_isolation_level VARCHAR(64),
                                      IN i_autocommit VARCHAR(16),
                                      IN i_savepoints INT,
                                      IN i_rb_savepoint INT,
                                      IN i_rel_savepoint INT,
                                      IN i_expected INT)
BEGIN
  DECLARE table_name VARCHAR(64);
  SET @thread_id       = i_thread_id;
  SET @event_id        = 0;
  SET @event_name      = i_event_name;
  SET @state           = i_state;
  SET @xid_format_id   = i_xid_format_id;
  SET @xid_gtrid       = i_xid_gtrid;
  SET @xid_bqual       = i_xid_bqual;
  SET @xa_state        = i_xa_state;
  SET @gtid            = i_gtid;
  SET @access_mode     = i_access_mode;
  SET @isolation_level = i_isolation_level;
  SET @autocommit      = i_autocommit;
  SET @savepoints      = i_savepoints;
  SET @rb_savepoint    = i_rb_savepoint;
  SET @rel_savepoint   = i_rel_savepoint;
  SET @expected        = i_expected;

  #
  # Build verification query based upon input parameters
  #

  IF i_table = 0 THEN
    SET table_name = 'performance_schema.events_transactions_current';
  ELSEIF i_table = 1 THEN
    SET table_name = 'performance_schema.events_transactions_history';
  ELSEIF i_table = 2 THEN
    SET table_name = 'performance_schema.events_transactions_history_long';
  ELSE
    SET table_name = 'performance_schema.events_transactions_history';
  END IF;

  SET @query = CONCAT('SELECT COUNT(*) INTO @actual FROM ', table_name, ' WHERE');

  IF i_thread_id != 0 THEN
    SET @query = CONCAT(@query, ' (thread_id = @thread_id)');
  END IF;
  IF i_event_name != '' THEN
    SET @query = CONCAT(@query, ' AND (event_name = @event_name)');
  END IF;
  IF i_state != '' THEN
    SET @query = CONCAT(@query, ' AND (state = @state)');
  END IF;
  IF i_xid_format_id != '' THEN
    SET @query = CONCAT(@query, ' AND (xid_format_id = @xid_format_id)');
  END IF;
  IF i_xid_gtrid != '' THEN
    SET @query = CONCAT(@query, ' AND (xid_gtrid = @xid_gtrid)');
  END IF;
  IF i_xid_bqual != '' THEN
    SET @query = CONCAT(@query, ' AND (xid_bqual = @xid_bqual)');
  END IF;
  IF i_xa_state != '' THEN
    SET @query = CONCAT(@query, ' AND (xa_state = @xa_state)');
  END IF;
  IF i_gtid = 'NULL' THEN
    SET @query = CONCAT(@query, ' AND (gtid IS NULL)');
  ELSEIF i_gtid != '' THEN
    SET @query = CONCAT(@query, ' AND (gtid = @gtid)');
  END IF;
  IF i_access_mode != '' THEN
    SET @query = CONCAT(@query, ' AND (access_mode = @access_mode)');
  END IF;
  IF i_isolation_level != '' THEN
    SET @query = CONCAT(@query, ' AND (isolation_level = @isolation_level)');
  END IF;
  IF i_autocommit != '' THEN
    SET @query = CONCAT(@query, ' AND (autocommit = @autocommit)');
  END IF;
  IF i_savepoints != 0 THEN
    SET @query = CONCAT(@query, ' AND (number_of_savepoints = @savepoints)');
  END IF;
  IF i_rb_savepoint != 0 THEN
    SET @query = CONCAT(@query, ' AND (number_of_rollback_to_savepoint = @rb_savepoint)');
  END IF;
  IF i_rel_savepoint != 0 THEN
    SET @query = CONCAT(@query, ' AND (number_of_release_savepoint = @rel_savepoint)');
  END IF;
 
  SET @query = CONCAT(@query, ' ORDER BY event_id;');

  ## DEBUG ## SELECT * FROM performance_schema.events_transactions_history ORDER BY event_id;
  ## SELECT @query AS "QUERY";
  PREPARE stmt1 FROM @query;
  EXECUTE stmt1;
  DEALLOCATE PREPARE stmt1;

  SELECT LPAD(@actual, 6, ' ') AS "ACTUAL", LPAD(@expected, 8, ' ') AS "EXPECTED";

  IF @actual != @expected THEN
    SELECT "" AS "ERROR: Row count mismatch";
    SELECT @query AS "VERIFIER QUERY:";
    SELECT "";

    SET @columns =                  ' LPAD(@thread_id, 9, " ")      AS thread_id,';
    SET @columns = CONCAT(@columns, ' LPAD(@event_id, 10, " ")      AS "..event_id",');
    SET @columns = CONCAT(@columns, ' RPAD(@event_name, 11, " ")    AS "event_name ",');
    SET @columns = CONCAT(@columns, ' RPAD(@state, 11, " ")         AS "state      ",');
    SET @columns = CONCAT(@columns, ' RPAD(@xid_format_id, 15, " ") AS "xid_format_id  ",');
    SET @columns = CONCAT(@columns, ' RPAD(@xid_gtrid, 15, " ")     AS "xid_gtrid      ",');
    SET @columns = CONCAT(@columns, ' RPAD(@xid_bqual, 15, " ")     AS "xid_bqual      ",');
    SET @columns = CONCAT(@columns, ' RPAD(@xa_state, 12, " ")      AS "xa_state    ",');
    SET @columns = CONCAT(@columns, ' RPAD(@gtid, 38, " ")          AS "gtid                                  ",');
    SET @columns = CONCAT(@columns, ' RPAD(@access_mode, 11, " ")   AS access_mode,');
    SET @columns = CONCAT(@columns, ' RPAD(@isolation_level, 16, " ") AS "isolation_level ",');
    SET @columns = CONCAT(@columns, ' RPAD(@autocommit, 10, " ")    AS autocommit,');
    SET @columns = CONCAT(@columns, ' LPAD(@savepoints, 10, " ")    AS savepoints,');
    SET @columns = CONCAT(@columns, ' LPAD(@rb_savepoint, 21, " ")  AS rollback_to_savepoint,');
    SET @columns = CONCAT(@columns, ' LPAD(@rel_savepoint, 17, " ") AS release_savepoint');
    SET @query2  = CONCAT('SELECT', ' LPAD(@expected, 13, " ")      AS ROWS_EXPECTED, ', @columns, ';');
#   SET @query2  = CONCAT('SELECT "        " AS EXPECTED, ', @columns, ';');
    PREPARE stmt2 FROM @query2;
    EXECUTE stmt2;
    DEALLOCATE PREPARE stmt2;

    SET @columns =                  ' LPAD(thread_id, 9, " ")        AS thread_id,';
    SET @columns = CONCAT(@columns, ' LPAD(event_id, 10, " ")        AS "..event_id",');
    SET @columns = CONCAT(@columns, ' RPAD(event_name, 11, " ")      AS "event_name ",');
    SET @columns = CONCAT(@columns, ' RPAD(state, 11, " ")           AS "state      ",');
    SET @columns = CONCAT(@columns, ' RPAD(IFNULL(xid_format_id, "NULL"), 15, " ") AS "xid_format_id  ",');
    SET @columns = CONCAT(@columns, ' RPAD(IFNULL(xid_gtrid, "NULL"), 15, " ") AS "xid_gtrid      ",');
    SET @columns = CONCAT(@columns, ' RPAD(IFNULL(xid_bqual, "NULL"), 15, " ") AS "xid_bqual      ",');
    SET @columns = CONCAT(@columns, ' RPAD(IFNULL(xa_state, "NULL"), 12, " ") AS "xa_state    ",');
    SET @columns = CONCAT(@columns, ' RPAD(IFNULL(gtid, "NULL"), 38, " ") AS "gtid                                  ",');
    SET @columns = CONCAT(@columns, ' RPAD(access_mode, 11, " ")     AS access_mode,');
    SET @columns = CONCAT(@columns, ' RPAD(isolation_level, 16, " ") AS "isolation_level ",');
    SET @columns = CONCAT(@columns, ' RPAD(autocommit, 10, " ")      AS autocommit,');
    SET @columns = CONCAT(@columns, ' LPAD(number_of_savepoints, 10, " ") AS savepoints,');
    SET @columns = CONCAT(@columns, ' LPAD(number_of_rollback_to_savepoint, 21, " ") AS rollback_to_savepoint,');
    SET @columns = CONCAT(@columns, ' LPAD(number_of_release_savepoint, 17, " ") AS release_savepoint');
#   SET @query3  = CONCAT('SELECT "        " AS "ACTUAL  ", ', @columns, ' FROM ', table_name, ' ORDER BY event_id;');
    SET @query3  = CONCAT('SELECT', ' LPAD(@actual, 13, " ")        AS "ROWS_ACTUAL  ",', @columns, ' FROM ', table_name, ' ORDER BY event_id;');
    PREPARE stmt3 FROM @query3;
    EXECUTE stmt3;
    DEALLOCATE PREPARE stmt3;

  END IF;
END$$

DELIMITER ;$$

--enable_result_log



