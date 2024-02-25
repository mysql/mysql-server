#
# Expected test structure:
#
# 1)
#
# update table performance_schema.setup_instruments,
# to set enabled and controlled memory instruments
# to use for the payload.
#
# 2)
#
# --source ../include/controlled_memory_setup.inc
#
# Optional:
#
# let $ctrl_mem_debug = TRUE;
#
# 3) (multiple sections)
#
# --connection con1
#
# <test payload>
#
# --connection con2
#
# <test payload>
#
# --source ../include/controlled_memory_dump.inc
#
# 4)
# --source ../include/controlled_memory_cleanup.inc
#
# Tests performed:
#
# - check that per thread stats are consistent with thread summaries
# - check that per statements stats are consistent with thread stats
# - check that digest summaries are consistent with statements

--disable_query_log

# To debug a test, override this default in the .test file
let $ctrl_mem_debug = FALSE;

CREATE TABLE test.enforced AS
  SELECT NAME FROM performance_schema.setup_instruments
    WHERE (find_in_set('controlled', FLAGS));

delimiter $$;

CREATE PROCEDURE test.verify_thread(
  inspect_tid BIGINT UNSIGNED,
  con_name varchar(10),
  debug BOOLEAN)
BEGIN
  DECLARE expected_ctrl_size bigint;
  DECLARE expected_ctrl_max bigint;
  DECLARE expected_total_size bigint;
  DECLARE expected_total_max bigint;
  DECLARE thread_ctrl_size bigint;
  DECLARE thread_ctrl_max bigint;
  DECLARE thread_total_size bigint;
  DECLARE thread_total_max bigint;
  DECLARE stmt_ctrl_max bigint;
  DECLARE stmt_total_max bigint;
  declare msg varchar(512);

  SELECT SUM(CURRENT_NUMBER_OF_BYTES_USED), SUM(HIGH_NUMBER_OF_BYTES_USED)
    FROM performance_schema.memory_summary_by_thread_by_event_name
    WHERE THREAD_ID = inspect_tid
    AND EVENT_NAME IN (SELECT * FROM test.enforced)
    INTO expected_ctrl_size, expected_ctrl_max;

  /* For empty test.enforced */
  IF (expected_ctrl_size is null)
  THEN
    SET expected_ctrl_size = 0;
  END IF;

  IF (expected_ctrl_max is null)
  THEN
    SET expected_ctrl_max = 0;
  END IF;

  SELECT SUM(CURRENT_NUMBER_OF_BYTES_USED), SUM(HIGH_NUMBER_OF_BYTES_USED)
    FROM performance_schema.memory_summary_by_thread_by_event_name
    WHERE THREAD_ID = inspect_tid
    INTO expected_total_size, expected_total_max;

  IF (debug)
  THEN
    SELECT THREAD_ID, EVENT_NAME, CURRENT_NUMBER_OF_BYTES_USED, HIGH_NUMBER_OF_BYTES_USED
      FROM performance_schema.memory_summary_by_thread_by_event_name
      WHERE THREAD_ID = inspect_tid
      AND HIGH_NUMBER_OF_BYTES_USED > 0;
  END IF;

  SELECT CONTROLLED_MEMORY, MAX_CONTROLLED_MEMORY, TOTAL_MEMORY, MAX_TOTAL_MEMORY
    FROM performance_schema.threads
    WHERE THREAD_ID = inspect_tid
    INTO thread_ctrl_size, thread_ctrl_max, thread_total_size, thread_total_max;

  IF (debug)
  THEN
    SELECT con_name as connection,
      expected_ctrl_size, thread_ctrl_size,
      expected_ctrl_max, thread_ctrl_max,
      expected_total_size, thread_total_size,
      expected_total_max, thread_total_max;
  END IF;

  IF (thread_ctrl_size <> expected_ctrl_size)
  THEN
    SET msg = concat("CTRL SIZE: expected ", expected_ctrl_size, " actual ", thread_ctrl_size);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  /* MAX(SUM(Xi)) <= SUM(MAX(Xi)) */
  IF (thread_ctrl_max > expected_ctrl_max)
  THEN
    SET msg = concat("CTRL MAX: expected ", expected_ctrl_max, " actual ", thread_ctrl_max);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  IF (thread_total_size <> expected_total_size)
  THEN
    SET msg = concat("TOTAL SIZE: expected ", expected_total_size, " actual ", thread_total_size);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  /* MAX(SUM(Xi)) <= SUM(MAX(Xi)) */
  IF (thread_total_max > expected_total_max)
  THEN
    SET msg = concat("TOTAL MAX: expected ", expected_total_max, " actual ", thread_total_max);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  SELECT con_name as connection, "threads PASSED" as check_status;

  SELECT MAX(MAX_CONTROLLED_MEMORY), MAX(MAX_TOTAL_MEMORY)
    from performance_schema.events_statements_history_long
    WHERE THREAD_ID = inspect_tid
    into stmt_ctrl_max, stmt_total_max;

  if (debug)
  then
    SELECT con_name as connection,
      stmt_ctrl_max, thread_ctrl_max,
      stmt_total_max, thread_total_max;
  end if;

  IF (stmt_ctrl_max > thread_ctrl_max)
  THEN
    SET msg = concat("STMT CTRL MAX: expected ", thread_ctrl_max, " actual ", stmt_ctrl_max);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  IF (stmt_total_max > thread_total_max)
  THEN
    SET msg = concat("STMT TOTAL MAX: expected ", thread_total_max, " actual ", stmt_total_max);
    SIGNAL sqlstate '05000' set message_text= msg;
  END IF;

  SELECT con_name as connection, "statement history PASSED" as check_status;

  SELECT now() as offending_ctrl_digests,
    d.DIGEST_TEXT, d.MAX_CONTROLLED_MEMORY, h.max_ctrl
    FROM performance_schema.events_statements_summary_by_digest d
    JOIN (
      SELECT DIGEST, MAX(MAX_CONTROLLED_MEMORY) as max_ctrl
        FROM performance_schema.events_statements_history_long
        WHERE THREAD_ID = inspect_tid
        GROUP BY DIGEST
    ) as h
    ON d.DIGEST = h.DIGEST
    WHERE (d.MAX_CONTROLLED_MEMORY < h.max_ctrl);

  SELECT now() as offending_total_digests,
    d.DIGEST_TEXT, d.MAX_TOTAL_MEMORY, h.max_total
    FROM performance_schema.events_statements_summary_by_digest d
    JOIN (
      SELECT DIGEST, MAX(MAX_TOTAL_MEMORY) as max_total
        FROM performance_schema.events_statements_history_long
        WHERE THREAD_ID = inspect_tid
        GROUP BY DIGEST
    ) as h
    ON d.DIGEST = h.DIGEST
    WHERE (d.MAX_TOTAL_MEMORY < h.max_total);
END
$$

delimiter ;$$

--enable_query_log

--echo # Switch to (con1, localhost, root, , )
connect (con1, localhost, root, , );

let $con1_thread_id= `select THREAD_ID from performance_schema.threads
  where PROCESSLIST_ID = connection_id()`;

--echo # Switch to (con2, localhost, root, , )
connect (con2, localhost, root, , );

let $con2_thread_id= `select THREAD_ID from performance_schema.threads
  where PROCESSLIST_ID = connection_id()`;

--connection default

--disable_query_log

UPDATE performance_schema.setup_threads SET ENABLED = 'NO';
UPDATE performance_schema.threads SET INSTRUMENTED = 'NO';

--eval UPDATE performance_schema.threads SET INSTRUMENTED = 'YES' WHERE THREAD_ID IN ($con1_thread_id, $con2_thread_id);

TRUNCATE TABLE performance_schema.events_statements_history_long;
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
TRUNCATE TABLE performance_schema.events_statements_summary_by_thread_by_event_name;

--enable_query_log
